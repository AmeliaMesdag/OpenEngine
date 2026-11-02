#pragma once

#include "TerrainECS/PageLevelRule.h"
#include "TerrainECS/PageTable.h"
#include "TerrainECS/PagedHeightSampler.h"
#include "Types/FlatMap.h"
#include "Types/Types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace GameEngine::PageStreaming
{
class PageResidencyManager;
class PageStoreAccess;
} // namespace GameEngine::PageStreaming

namespace GameEngine::TerrainECS
{

struct PageStreamCounts;
struct TerrainSurfacePageSource;

/// What a platform spends on surface pages (design D5): the surface field's cache slots. Each slot
/// holds one page of every plane a terrain has (PREMISE "The physical caches and the budget").
struct SurfacePageProfile
{
    uint32 Slots = 0;
};

/// Desktop: 2,048 slots, 168 MiB for the splat and BC5 normal planes, 366 MiB with grass control and color.
inline constexpr SurfacePageProfile kDesktopSurfacePages{2048u};
/// Steam Deck: half the desktop slots.
inline constexpr SurfacePageProfile kSteamDeckSurfacePages{1024u};

/// The profile of the platform this build targets.
SurfacePageProfile PlatformSurfacePageProfile();

/// The surface pages' level rule target: one texel a pixel (lambda_s, planets design 3.5).
inline constexpr float32 kSurfacePageTargetPixels = 1.0f;

/// The field id of the surface pages in the residency manager.
inline constexpr uint32 kSurfacePageField = 1u;

/// What a terrain whose surface is paged (TerrainSurfaceIsPaged) asks for this frame.
struct SurfacePagedRequest
{
    uint32 Terrain = 0; ///< the terrain's render handle (TerrainHandle::Index, ::Generation)
    uint32 TerrainGeneration = 0;
    std::string_view Name; ///< the terrain entity's name, for the log lines (empty: its render index)
    const TerrainSurfacePageSource* Source = nullptr;
    PageRequestTerrain Shape; ///< its pyramid in world meters; CacheSlots is set by TerrainSurfacePages
};

/// One page of every plane for a surface cache slot, uploaded by the GPU side this frame.
struct SurfacePageUpload
{
    uint32 Slot = 0;
    uint32 PlaneMask = 0;     ///< the planes in Bytes (TerrainSurfacePageSource::PlaneMask)
    std::vector<uint8> Bytes; ///< each plane's PagePayloadBytes in turn, in PageFieldKind order
};

/// The surface pages of every surface-paged terrain, on the main thread: each terrain's stream over
/// its plane stores, requested over the view wedge at one texel a pixel, its page table, and the slot
/// uploads the GPU side takes each frame. The streams live in the residency of TerrainPages, its owner.
class TerrainSurfacePages
{
public:
    /// Streams in `residency`, under stream ids taken from `nextStream`, the owner's one counter.
    TerrainSurfacePages(PageStreaming::PageResidencyManager& residency, const SurfacePageProfile& profile,
                        const PageTableBudget& tableBudget, uint32& nextStream);

    /// The pages `request`'s terrain needs for `cameras` this frame. A terrain not requested between
    /// two Updates is forgotten at the second.
    void Request(const SurfacePagedRequest& request, std::span<const PageRequestCamera> cameras,
                 const PageLevelView& view);

    /// (Re)starts the surface field's cache for the renderer's frames in flight, empties every
    /// page table and keeps each stream's loaded pages.
    void ConfigureField(uint32 framesInFlight);
    /// Before the residency's Update: ends the streams of the terrains not requested since the last call.
    void ForgetUnrequested();
    /// After the residency's Update: brings each terrain's page table up to date and gathers the uploads.
    void Collect();

    /// This frame's slot uploads. The GPU side takes them after every Update: the next Update clears
    /// what was not taken.
    std::vector<SurfacePageUpload>& Uploads() { return m_Uploads; }
    /// The pages whose table entry, fade or slot texels changed since the caller last drained them
    /// (TerrainPageFeature::Publish moves them out), one rect a page; a reset covers its whole terrain.
    std::vector<PageChangeRect>& ChangeRects() { return m_ChangeRects; }
    /// The surface page-table words of a terrain by render handle index, their version (bumped on
    /// every change) and the terrain's planes (TerrainSurfacePageSource::PlaneMask), or null.
    const std::vector<uint32>* TableWords(uint32 terrain, uint64& version, uint32& planeMask) const;
    /// The render handle indices of the terrains whose surface is paged.
    void PagedTerrains(std::vector<uint32>& out) const;
    /// Appends each surface stream's wanted and resident pages per level to `out`, as of the last Update.
    void StreamCounts(std::vector<PageStreamCounts>& out) const;
    /// The surface field's cache geometry: every plane cache packs its slots in these rows.
    const PageCacheGeometry& Geometry() const { return m_Geometry; }

private:
    struct PagedSurface
    {
        uint32 Stream = 0;
        uint32 Generation = 0;
        std::string Label;
        std::vector<const PageStreaming::PageStoreAccess*> Planes; ///< the source its stream holds
        uint32 PlaneMask = 0;
        PageRequestTerrain Shape;
        PageRequester Requester;
        PageTable Table;
        std::vector<uint32> Words;
        uint64 Version = 0;
        bool Requested = false;
        std::vector<PageStreaming::PageAddress> WantedPages; ///< the last request's pages
        std::string LoggedResidency;                         ///< GE_CBT_VALIDATE: the residency line last logged
    };

    void Configure(PagedSurface& surface, const SurfacePagedRequest& request);
    void ResetTable(uint32 terrain, PagedSurface& surface);
    void CollectStreamChanges(uint32 terrain, PagedSurface& surface);

    PageStreaming::PageResidencyManager& m_Residency;
    SurfacePageProfile m_Profile;
    PageTableBudget m_TableBudget;
    PageCacheGeometry m_Geometry;
    uint32& m_NextStream;
    uint32 m_FramesInFlight = 0;
    uint64 m_TableVersion = 0;
    FlatMap<uint32, PagedSurface> m_Surfaces; // by render index: a few terrains
    std::vector<PageStreaming::PageWant> m_Wants; // reused per request
    std::vector<SurfacePageUpload> m_Uploads;
    std::vector<PageChangeRect> m_ChangeRects;
    std::vector<uint32> m_Forgotten; // reused per Update
    std::vector<PageStreaming::PageAddress> m_LateEntries; // reused per Update
};

} // namespace GameEngine::TerrainECS
