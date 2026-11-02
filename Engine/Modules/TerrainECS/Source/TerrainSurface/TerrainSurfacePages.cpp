#include "TerrainECS/TerrainSurface/TerrainSurfacePages.h"

#include "TerrainECS/TerrainSurface/TerrainSurfacePageSource.h"
#include "TerrainECS/TerrainAtlas.h"
#include "TerrainECS/TerrainHeightSource.h"
#include "TerrainECS/TerrainPages.h"
#include "PageStreaming/PageCache.h"
#include "PageStreaming/PageResidency.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <format>

namespace GameEngine::TerrainECS
{

SurfacePageProfile PlatformSurfacePageProfile()
{
#if defined(PLATFORM_STEAMDECK)
    return kSteamDeckSurfacePages;
#else
    return kDesktopSurfacePages;
#endif
}

bool TerrainSurfaceIsPaged(const TerrainHeightSourceInputs& height, const TerrainSurfacePageSource& surface)
{
    return TerrainHeightIsPaged(height) && !surface.Planes.empty() && surface.PinnedLevelsBaked;
}

TerrainSurfacePages::TerrainSurfacePages(PageStreaming::PageResidencyManager& residency,
                                         const SurfacePageProfile& profile, const PageTableBudget& tableBudget,
                                         uint32& nextStream)
    : m_Residency(residency)
    , m_Profile(profile)
    , m_TableBudget(tableBudget)
    , m_Geometry(MakePageCacheGeometry(profile.Slots))
    , m_NextStream(nextStream)
{
}

void TerrainSurfacePages::ResetTable(uint32 terrain, PagedSurface& surface)
{
    surface.Table.Reset(surface.Shape.SamplesX, surface.Shape.SamplesZ, 1u, m_Profile.Slots, m_TableBudget);
    PackPageTableWords(surface.Table, m_Geometry, surface.Words);
    surface.Version = ++m_TableVersion;
    m_ChangeRects.push_back(PageChangeRect{terrain});
}

void TerrainSurfacePages::Configure(PagedSurface& surface, const SurfacePagedRequest& request)
{
    const TerrainSurfacePageSource& source = *request.Source;
    if (surface.Stream == 0)
        surface.Stream = m_NextStream++;
    surface.Label = request.Name.empty() ? std::format("terrain {}", request.Terrain) : std::string(request.Name);
    surface.Planes.clear();
    for (const std::shared_ptr<const PageStreaming::PageStoreAccess>& plane : source.Planes)
        surface.Planes.push_back(plane.get());
    surface.PlaneMask = source.PlaneMask;
    surface.Shape = request.Shape;
    surface.Shape.CacheSlots = m_Profile.Slots;
    surface.Requester.Configure(surface.Shape, PageRequestArea::Wedge);
    ResetTable(request.Terrain, surface);
    m_Residency.Configure(surface.Stream, kSurfacePageField,
                          PageStreaming::PageSource{nullptr, nullptr, source.Planes});
    Logger::Log::Info("Terrain pages: {} (terrain {} generation {}) streams its surface pages ({} planes, {} x {} "
                      "samples, {} levels)",
                      surface.Label, request.Terrain, request.TerrainGeneration, source.Planes.size(),
                      surface.Shape.SamplesX, surface.Shape.SamplesZ, surface.Table.Levels().size());
}

void TerrainSurfacePages::Request(const SurfacePagedRequest& request, std::span<const PageRequestCamera> cameras,
                                  const PageLevelView& view)
{
    if (m_FramesInFlight == 0 || request.Source == nullptr)
        return; // the field's cache is configured by the first Update
    const bool inserted = !m_Surfaces.Contains(request.Terrain);
    PagedSurface& surface = m_Surfaces.GetOrInsert(request.Terrain);
    surface.Generation = request.TerrainGeneration;
    const std::vector<std::shared_ptr<const PageStreaming::PageStoreAccess>>& planes = request.Source->Planes;
    const bool samePlanes = std::ranges::equal(
        surface.Planes, planes, {}, {}, [](const std::shared_ptr<const PageStreaming::PageStoreAccess>& plane) {
            return plane.get();
        });
    if (inserted || !samePlanes || request.Shape.SamplesX != surface.Shape.SamplesX ||
        request.Shape.SamplesZ != surface.Shape.SamplesZ)
    {
        Configure(surface, request);
    }
    else if (!SamePagePlacement(request.Shape, surface.Shape))
    {
        // Moved or rescaled: the resident pages are keyed by terrain UV and stay valid.
        surface.Shape = request.Shape;
        surface.Shape.CacheSlots = m_Profile.Slots;
        surface.Requester.Configure(surface.Shape, PageRequestArea::Wedge);
    }
    surface.Requested = true;
    if (!surface.Requester.Build(cameras, view, m_Wants))
        return; // a parked camera: the last request stands
    m_Residency.Request(surface.Stream, m_Wants);
    surface.WantedPages.clear();
    for (const PageStreaming::PageWant& want : m_Wants)
        surface.WantedPages.push_back(want.Address);
}

void TerrainSurfacePages::ConfigureField(uint32 framesInFlight)
{
    m_FramesInFlight = framesInFlight;
    m_Residency.ConfigureField(kSurfacePageField, m_Profile.Slots, framesInFlight, kAtlasUpgradeFadeSeconds);
    for (auto& [key, surface] : m_Surfaces)
        ResetTable(key, surface); // its stream keeps its pages; their slots are gone
}

void TerrainSurfacePages::ForgetUnrequested()
{
    m_Uploads.clear();
    m_Forgotten.clear();
    for (auto& [key, surface] : m_Surfaces)
    {
        if (!surface.Requested)
            m_Forgotten.push_back(key);
        surface.Requested = false;
    }
    for (uint32 key : m_Forgotten)
    {
        PagedSurface& forgotten = *m_Surfaces.Find(key);
        Logger::Log::Info("Terrain pages: {} (terrain {} generation {}) stops streaming its surface pages",
                          forgotten.Label, key, forgotten.Generation);
        m_Residency.Forget(forgotten.Stream);
        m_Surfaces.Erase(key);
    }
}

void TerrainSurfacePages::CollectStreamChanges(uint32 terrain, PagedSurface& surface)
{
    const PageStreaming::PageCache& cache = m_Residency.FieldCache(kSurfacePageField);
    const uint32 writes = UpdatePageTable(cache, surface.Stream, surface.Table);
    // The fragment reads the table each frame: a late entry needs no dirty rect, only its change rect.
    m_LateEntries.clear();
    surface.Table.TakeLateEntries(m_LateEntries);
    if (writes != 0u)
    {
        PackPageTableWords(surface.Table, m_Geometry, surface.Words);
        surface.Version = ++m_TableVersion;
    }
    AppendPageChangeRects(terrain, surface.Stream, surface.Table.Levels(), cache, m_LateEntries, m_ChangeRects);
    for (const PageStreaming::PageUploadRequest& upload : cache.Uploads())
    {
        if (upload.Page.Stream != surface.Stream)
            continue;
        if (const std::vector<uint8>* bytes = m_Residency.Bytes(surface.Stream, upload.Page.Address))
            m_Uploads.push_back(SurfacePageUpload{upload.Slot, surface.PlaneMask, *bytes});
    }
}

void TerrainSurfacePages::Collect()
{
    for (auto& [key, surface] : m_Surfaces)
    {
        CollectStreamChanges(key, surface);
        LogPageResidencyChange(surface.Label, " surface", surface.Table, surface.WantedPages, surface.LoggedResidency);
    }
}

const std::vector<uint32>* TerrainSurfacePages::TableWords(uint32 terrain, uint64& version, uint32& planeMask) const
{
    const PagedSurface* surface = m_Surfaces.Find(terrain);
    if (!surface)
        return nullptr;
    version = surface->Version;
    planeMask = surface->PlaneMask;
    return &surface->Words;
}

void TerrainSurfacePages::PagedTerrains(std::vector<uint32>& out) const
{
    out.clear();
    for (const auto& [key, surface] : m_Surfaces)
        out.push_back(key);
}

void TerrainSurfacePages::StreamCounts(std::vector<PageStreamCounts>& out) const
{
    const PageStreaming::PageCache& cache = m_Residency.FieldCache(kSurfacePageField);
    for (const auto& [key, surface] : m_Surfaces)
    {
        PageStreamCounts& counts = out.emplace_back();
        counts.Label = surface.Label;
        counts.Surface = true;
        CountWantedPages(cache, surface.Stream, surface.WantedPages,
                         static_cast<uint32>(surface.Table.Levels().size()), counts);
    }
}

} // namespace GameEngine::TerrainECS
