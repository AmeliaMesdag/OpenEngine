#include "TerrainECS/TerrainPageFeature.h"

#include "PageStreaming/PageStore/PageStoreFormat.h"
#include "Logger/Logger.h"
#include "Rendering/Core/CommandList.h"
#include "Rendering/Core/Device.h"

#include <algorithm>
#include <array>

namespace GameEngine::TerrainECS
{

namespace
{

constexpr std::size_t kPageBytes = static_cast<std::size_t>(PageStreaming::kPageSampleCount) * sizeof(float32);
// A surface upload's bytes start on a 16-byte boundary in the staging: a BC5 copy's buffer offset is a
// multiple of its 16-byte block, and an RG8 plane (33,800 bytes) breaks that alignment for the planes after it.
constexpr std::size_t kSurfaceStagingAlignment = PageStreaming::kBc5BlockBytes;

constexpr std::array<const char*, kSurfacePlanes.size()> kPlaneCacheNames = {
    "Terrain.SurfacePageCache.Splat", "Terrain.SurfacePageCache.Normal", "Terrain.SurfacePageCache.GrassControl",
    "Terrain.SurfacePageCache.Color"};

Rendering::TextureFormat PlaneTextureFormat(PageStreaming::PageFieldFormat format)
{
    switch (format)
    {
    case PageStreaming::PageFieldFormat::Bc5: return Rendering::TextureFormat::BC5_UNORM;
    case PageStreaming::PageFieldFormat::Rg8: return Rendering::TextureFormat::R8G8_UNORM;
    case PageStreaming::PageFieldFormat::Rgba8Srgb: return Rendering::TextureFormat::RGBA8_SRGB;
    default: return Rendering::TextureFormat::RGBA8_UNORM;
    }
}

// The cells of a grid of `cells` a side over terrain UV that rect [minU, maxU] x [minV, maxV] meets.
struct GridCells
{
    uint32 X0 = 0;
    uint32 X1 = 0;
    uint32 Z0 = 0;
    uint32 Z1 = 0;
};

GridCells CellsOf(const Mathematics::Rect& uv, uint32 cells)
{
    const float32 extent = static_cast<float32>(cells);
    const auto cell = [extent](float32 t) { return static_cast<uint32>(std::clamp(t * extent, 0.0f, extent - 1.0f)); };
    return GridCells{cell(uv.X), cell(uv.Right()), cell(uv.Y), cell(uv.Bottom())};
}

} // namespace

TerrainPageFeature::~TerrainPageFeature()
{
    if (!m_Device)
        return;
    for (const RetiringBuffer& retiring : m_Retiring)
        m_Device->DestroyBuffer(retiring.Buffer);
    m_TableRing.Release(m_Device);
    if (m_Staging.IsValid())
        m_Device->DestroyBuffer(m_Staging);
    std::array<Rendering::TextureHandle, kSurfacePlanes.size() + 1u> textures{m_Cache};
    for (std::size_t plane = 0; plane < kSurfacePlanes.size(); ++plane)
        textures[plane + 1u] = m_Planes[plane].Texture;
    for (Rendering::TextureHandle texture : textures)
        if (texture.IsValid())
            m_Device->DestroyTexture(texture);
}

void TerrainPageFeature::PublishTable(Table& table, const std::vector<uint32>* words, uint64 version,
                                      uint32 planeMask)
{
    if (words == nullptr || (table.Version == version && table.Words))
        return;
    table.Words = std::make_shared<const std::vector<uint32>>(*words);
    table.Version = version;
    table.PlaneMask = planeMask;
}

void TerrainPageFeature::ForgetUnpaged(TableMap& tables, std::span<const uint32> paged)
{
    std::erase_if(tables,
                  [&](const auto& entry) { return std::find(paged.begin(), paged.end(), entry.first) == paged.end(); });
}

void TerrainPageFeature::Publish(TerrainPages& pages)
{
    std::vector<uint32> paged;
    std::vector<uint32> surfacePaged;
    pages.PagedTerrains(paged);
    TerrainSurfacePages& surface = pages.Surface();
    surface.PagedTerrains(surfacePaged);
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CacheDim = pages.Geometry().CacheDim;
    m_SurfaceSlotsPerRow = surface.Geometry().SlotsPerRow;
    for (HeightPageUpload& upload : pages.Uploads())
        m_PublishedUploads.push_back(std::move(upload));
    pages.Uploads().clear();
    for (SurfacePageUpload& upload : surface.Uploads())
        m_PublishedSurfaceUploads.push_back(std::move(upload));
    surface.Uploads().clear();
    m_PublishedChanges.insert(m_PublishedChanges.end(), pages.ChangeRects().begin(), pages.ChangeRects().end());
    pages.ChangeRects().clear();
    m_PublishedChanges.insert(m_PublishedChanges.end(), surface.ChangeRects().begin(), surface.ChangeRects().end());
    surface.ChangeRects().clear();
    ForgetUnpaged(m_PublishedTables.Height, paged);
    ForgetUnpaged(m_PublishedTables.Surface, surfacePaged);
    for (uint32 terrain : paged)
    {
        uint64 version = 0;
        const std::vector<uint32>* words = pages.TableWords(terrain, version);
        PublishTable(m_PublishedTables.Height[terrain], words, version, 0u);
    }
    for (uint32 terrain : surfacePaged)
    {
        uint64 version = 0;
        uint32 planeMask = 0;
        const std::vector<uint32>* words = surface.TableWords(terrain, version, planeMask);
        PublishTable(m_PublishedTables.Surface[terrain], words, version, planeMask);
    }
}

void TerrainPageFeature::Latch(Rendering::IDevice& device, uint64 rgFrame)
{
    if (rgFrame == m_LatchedFrame)
        return;
    m_LatchedFrame = rgFrame;
    m_Device = &device;
    m_CacheReadable = m_Cache.IsValid() && m_CacheCleared;
    for (PlaneCache& plane : m_Planes)
        plane.Readable = plane.Texture.IsValid() && plane.Written;

    const uint64 retireAfter = static_cast<uint64>(device.GetFramesInFlight()) + 1u;
    std::erase_if(m_Retiring, [&](const RetiringBuffer& retiring) {
        if (rgFrame < retiring.Frame + retireAfter)
            return false;
        device.DestroyBuffer(retiring.Buffer);
        return true;
    });

    bool newUploads = false;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        newUploads = !m_PublishedUploads.empty() || !m_PublishedSurfaceUploads.empty();
        for (HeightPageUpload& upload : m_PublishedUploads)
            m_LatchedUploads.push_back(std::move(upload));
        m_PublishedUploads.clear();
        for (SurfacePageUpload& upload : m_PublishedSurfaceUploads)
            m_LatchedSurfaceUploads.push_back(std::move(upload));
        m_PublishedSurfaceUploads.clear();
        m_LatchedTables = m_PublishedTables; // one shared pointer per paged terrain and field
        m_LatchedChanges.insert(m_LatchedChanges.end(), m_PublishedChanges.begin(), m_PublishedChanges.end());
        m_PublishedChanges.clear();
        m_LatchedCacheDim = m_CacheDim;
        m_LatchedSurfaceSlotsPerRow = m_SurfaceSlotsPerRow;
    }
    if (m_LatchedUploads.empty() && m_LatchedSurfaceUploads.empty())
    {
        m_CommittedTables = m_LatchedTables;
        CommitChanges();
        return;
    }
    if (newUploads || !m_Staging.IsValid())
        StageUploads();
}

void TerrainPageFeature::StageUploads()
{
    RetireStaging();
    EnsureCache();
    EnsurePlaneCaches();
    if (!m_LatchedUploads.empty() && !m_Cache.IsValid())
        return;
    std::size_t bytes = m_LatchedUploads.size() * kPageBytes;
    m_SurfaceStagingOffsets.clear();
    for (const SurfacePageUpload& upload : m_LatchedSurfaceUploads)
    {
        bytes = (bytes + kSurfaceStagingAlignment - 1u) / kSurfaceStagingAlignment * kSurfaceStagingAlignment;
        m_SurfaceStagingOffsets.push_back(bytes);
        bytes += upload.Bytes.size();
    }
    m_Staging = m_Device->CreateUploadBuffer(bytes, "Terrain.PageStaging");
    if (!m_Staging.IsValid())
    {
        Logger::Log::Warning("Terrain pages: no staging for {} height and {} surface page uploads ({} bytes); the page "
                             "tables keep their last uploaded state and the uploads retry next frame",
                             m_LatchedUploads.size(), m_LatchedSurfaceUploads.size(), bytes);
        return;
    }
    for (std::size_t i = 0; i < m_LatchedUploads.size(); ++i)
        m_Device->UpdateBuffer(m_Staging, i * kPageBytes, kPageBytes, m_LatchedUploads[i].Bytes.data());
    for (std::size_t i = 0; i < m_LatchedSurfaceUploads.size(); ++i)
    {
        const std::vector<uint8>& payload = m_LatchedSurfaceUploads[i].Bytes;
        m_Device->UpdateBuffer(m_Staging, m_SurfaceStagingOffsets[i], payload.size(), payload.data());
    }
}

void TerrainPageFeature::RetireStaging()
{
    if (m_Staging.IsValid())
        m_Retiring.push_back({m_Staging, m_LatchedFrame});
    m_Staging = {};
}

TerrainPageTableRing::Slot TerrainPageFeature::BindTableRing(uint32 terrain, uint32 generation)
{
    std::span<const uint32> heightWords;
    std::span<const uint32> surfaceWords;
    TerrainPageTableRing::Key key{terrain, generation, 0, 0};
    if (m_Device == nullptr || !FindTable(m_CommittedTables.Height, terrain, heightWords, key.HeightVersion))
        return {};
    if (!FindTable(m_CommittedTables.Surface, terrain, surfaceWords, key.SurfaceVersion))
        key.SurfaceVersion = 0;
    Rendering::BufferHandle retired;
    const TerrainPageTableRing::Slot slot =
        m_TableRing.Bind(*m_Device, key, heightWords, surfaceWords, m_Device->GetFrameIndex(), retired);
    if (retired.IsValid())
        m_Retiring.push_back({retired, m_LatchedFrame});
    return slot;
}

void TerrainPageFeature::CommitLatch()
{
    if (!m_Staging.IsValid())
        return;
    m_CommittedTables = m_LatchedTables;
    CommitChanges();
}

void TerrainPageFeature::CommitChanges()
{
    if (m_LatchedChanges.empty())
        return;
    ++m_ChangeSerial;
    for (const PageChangeRect& rect : m_LatchedChanges)
    {
        std::vector<uint64>& grid = m_ChangeGrids.GetOrInsert(rect.Terrain);
        grid.resize(static_cast<std::size_t>(kChangeGridCells) * kChangeGridCells, 0u);
        const GridCells cells = CellsOf(rect.UV, kChangeGridCells);
        for (uint32 z = cells.Z0; z <= cells.Z1; ++z)
            std::fill_n(grid.begin() + z * kChangeGridCells + cells.X0, cells.X1 - cells.X0 + 1u, m_ChangeSerial);
    }
    m_LatchedChanges.clear();
}

uint64 TerrainPageFeature::ChangeSerialOver(uint32 terrain, const Mathematics::Rect& uv) const
{
    const std::vector<uint64>* grid = m_ChangeGrids.Find(terrain);
    if (grid == nullptr || uv.Width < 0.0f || uv.Height < 0.0f)
        return 0u;
    const GridCells cells = CellsOf(uv, kChangeGridCells);
    uint64 latest = 0;
    for (uint32 z = cells.Z0; z <= cells.Z1; ++z)
        for (uint32 x = cells.X0; x <= cells.X1; ++x)
            latest = std::max(latest, (*grid)[z * kChangeGridCells + x]);
    return latest;
}

void TerrainPageFeature::EnsureCache()
{
    if (m_Cache.IsValid() || m_LatchedCacheDim == 0)
        return;
    m_Cache = CreateCacheTexture(m_LatchedCacheDim, Rendering::TextureFormat::R32_FLOAT, "Terrain.HeightPageCache");
    m_CacheCleared = false;
    m_CacheReadable = false;
}

void TerrainPageFeature::EnsurePlaneCaches()
{
    uint32 planes = 0;
    for (const SurfacePageUpload& upload : m_LatchedSurfaceUploads)
        planes |= upload.PlaneMask;
    const uint32 slotsPerRow = m_LatchedSurfaceSlotsPerRow;
    for (std::size_t plane = 0; plane < kSurfacePlanes.size(); ++plane)
    {
        if (m_Planes[plane].Texture.IsValid() || ((planes >> plane) & 1u) == 0u || slotsPerRow == 0)
            continue;
        const PageStreaming::PageFieldFormat format = SurfacePlaneFormat(kSurfacePlanes[plane]);
        const uint32 dim = slotsPerRow * PageStreaming::PageStrideTexels(format);
        m_Planes[plane] = PlaneCache{CreateCacheTexture(dim, PlaneTextureFormat(format), kPlaneCacheNames[plane])};
        if (!m_Planes[plane].Texture.IsValid())
            Logger::Log::Warning("Terrain pages: the {} surface page cache ({} x {}) was not created; the plane's "
                                 "pages do not upload",
                                 SurfacePlaneName(kSurfacePlanes[plane]), dim, dim);
    }
}

Rendering::TextureHandle TerrainPageFeature::CreateCacheTexture(uint32 dim, Rendering::TextureFormat format,
                                                                const char* name)
{
    Rendering::TextureDesc desc{};
    desc.width = dim;
    desc.height = dim;
    desc.format = static_cast<uint32_t>(format);
    desc.usage = static_cast<uint32_t>(Rendering::TextureUsage::ShaderResource) |
                 static_cast<uint32_t>(Rendering::TextureUsage::TransferDst);
    desc.persistent = true;
    desc.debugName = name;
    return m_Device->CreateTexture(desc);
}

void TerrainPageFeature::EndCopies(Rendering::CommandList& cl, Rendering::TextureHandle texture)
{
    cl.Barrier(Rendering::ResourceBarrier::CreateTextureBarrier(texture, Rendering::ResourceState::CopyDest,
                                                                Rendering::ResourceState::ShaderResource));
}

void TerrainPageFeature::FlushSurfaceUploads(Rendering::CommandList& cl)
{
    const uint32 slotsPerRow = m_LatchedSurfaceSlotsPerRow;
    std::array<bool, kSurfacePlanes.size()> copied{};
    for (std::size_t i = 0; i < m_LatchedSurfaceUploads.size(); ++i)
    {
        const SurfacePageUpload& upload = m_LatchedSurfaceUploads[i];
        std::size_t offset = m_SurfaceStagingOffsets[i];
        for (std::size_t plane = 0; plane < kSurfacePlanes.size(); ++plane)
        {
            if (((upload.PlaneMask >> plane) & 1u) == 0u)
                continue;
            const PageStreaming::PageFieldFormat format = SurfacePlaneFormat(kSurfacePlanes[plane]);
            const uint32 stride = PageStreaming::PageStrideTexels(format);
            const PlaneCache& cache = m_Planes[plane];
            if (cache.Texture.IsValid())
            {
                // The first copy into a new texture discards it: no slot is read before its own upload.
                const Rendering::ResourceState resting =
                    cache.Written ? Rendering::ResourceState::ShaderResource : Rendering::ResourceState::Undefined;
                cl.CopyBufferToTextureSubresource(m_Staging, cache.Texture, 0, 0, stride, stride, offset, 0, 1, 0,
                                                  (upload.Slot % slotsPerRow) * stride,
                                                  (upload.Slot / slotsPerRow) * stride, resting);
                copied[plane] = true;
            }
            offset += PageStreaming::PagePayloadBytes(format);
        }
    }
    for (std::size_t plane = 0; plane < kSurfacePlanes.size(); ++plane)
    {
        if (!copied[plane])
            continue;
        EndCopies(cl, m_Planes[plane].Texture);
        m_Planes[plane].Written = true;
    }
    m_LatchedSurfaceUploads.clear();
    m_SurfaceStagingOffsets.clear();
}

void TerrainPageFeature::FlushUploads(Rendering::CommandList& cl)
{
    if (!m_Staging.IsValid())
        return;
    FlushSurfaceUploads(cl);
    if (!m_Cache.IsValid() || m_LatchedUploads.empty())
    {
        RetireStaging();
        return;
    }
    Rendering::ResourceState resting = Rendering::ResourceState::ShaderResource;
    if (!m_CacheCleared)
    {
        const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        cl.ClearColorImageSubresource(m_Cache, 0, 0, zero);
        resting = Rendering::ResourceState::CopyDest;
        m_CacheCleared = true;
    }

    const uint32 slotsPerRow = m_LatchedCacheDim / PageStreaming::kPageStrideSamples;
    for (std::size_t i = 0; i < m_LatchedUploads.size(); ++i)
    {
        const uint32 slot = m_LatchedUploads[i].Slot;
        cl.CopyBufferToTextureSubresource(m_Staging, m_Cache, 0, 0, PageStreaming::kPageStrideSamples,
                                          PageStreaming::kPageStrideSamples, i * kPageBytes,
                                          PageStreaming::kPageStrideSamples * sizeof(float32), 1, 0,
                                          (slot % slotsPerRow) * PageStreaming::kPageStrideSamples,
                                          (slot / slotsPerRow) * PageStreaming::kPageStrideSamples, resting);
        resting = Rendering::ResourceState::CopyDest;
    }
    EndCopies(cl, m_Cache);
    RetireStaging();
    m_LatchedUploads.clear();
}

bool TerrainPageFeature::FindTable(const TableMap& tables, uint32 terrain, std::span<const uint32>& words,
                                   uint64& version)
{
    const auto it = tables.find(terrain);
    if (it == tables.end() || !it->second.Words || it->second.Words->empty())
        return false;
    words = *it->second.Words;
    version = it->second.Version;
    return true;
}

bool TerrainPageFeature::LatchedTable(uint32 terrain, std::span<const uint32>& words, uint64& version) const
{
    return FindTable(m_CommittedTables.Height, terrain, words, version);
}

bool TerrainPageFeature::LatchedSurfaceTable(uint32 terrain, std::span<const uint32>& words, uint64& version) const
{
    return FindTable(m_CommittedTables.Surface, terrain, words, version);
}

Rendering::TextureHandle TerrainPageFeature::SurfacePlaneTexture(uint32 terrain,
                                                                 PageStreaming::PageFieldKind plane) const
{
    const auto table = m_CommittedTables.Surface.find(terrain);
    if (table == m_CommittedTables.Surface.end())
        return {};
    for (std::size_t index = 0; index < kSurfacePlanes.size(); ++index)
    {
        if (kSurfacePlanes[index] != plane)
            continue;
        const bool terrainHasPlane = ((table->second.PlaneMask >> index) & 1u) != 0u;
        return terrainHasPlane && m_Planes[index].Readable ? m_Planes[index].Texture : Rendering::TextureHandle{};
    }
    return {};
}

void TerrainPageFeature::OnDeviceRebuilt(Rendering::IDevice* device)
{
    // Every handle died with the old device: forget them without destroying them.
    m_Device = device;
    m_Cache = {};
    m_CacheCleared = false;
    m_CacheReadable = false;
    m_Staging = {};
    m_Retiring.clear();
    m_TableRing.Release(nullptr);
    m_Planes = {};
    m_LatchedUploads.clear();
    m_LatchedSurfaceUploads.clear();
    m_SurfaceStagingOffsets.clear();
    m_LatchedTables = {};
    m_CommittedTables = {};
    m_LatchedChanges.clear();
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_PublishedUploads.clear();
        m_PublishedSurfaceUploads.clear();
        m_PublishedChanges.clear();
    }
    m_DeviceEpoch.fetch_add(1, std::memory_order_acq_rel);
}

} // namespace GameEngine::TerrainECS
