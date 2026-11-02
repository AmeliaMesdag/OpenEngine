#include "TerrainECS/TerrainPageTableRing.h"
#include "Logger/Logger.h"
#include "Rendering/Core/Device.h"

#include <cstring>

namespace GameEngine::TerrainECS
{
namespace
{
// A slot grows in whole steps, so a table that grows a little does not reallocate the ring each time.
constexpr uint32 kSlotWordsStep = 4096u;
static_assert(TerrainPageTableRing::kSlots == Rendering::IDevice::kMaxSupportedFramesInFlight);
} // namespace

TerrainPageTableRing::Slot TerrainPageTableRing::Bind(Rendering::IDevice& device, const Key& key,
                                                      std::span<const uint32> heightWords,
                                                      std::span<const uint32> surfaceWords, uint32 frameSlot,
                                                      Rendering::BufferHandle& retired)
{
    retired = {};
    const uint32 slot = frameSlot % kSlots;
    const uint32 words = static_cast<uint32>(heightWords.size() + surfaceWords.size());
    if (!m_Buffer.IsValid() || words > m_SlotWords)
    {
        const uint32 slotWords = (words + kSlotWordsStep - 1u) / kSlotWordsStep * kSlotWordsStep;
        Rendering::BufferDesc desc{};
        desc.size = static_cast<size_t>(kSlots) * slotWords * sizeof(uint32);
        desc.usage = static_cast<uint32>(Rendering::BufferUsage::Storage);
        desc.memoryUsage = Rendering::BufferMemoryUsage::Upload;
        desc.persistent = true;
        desc.debugName = "Terrain.PageTableRing";
        const Rendering::BufferHandle next = device.CreateBuffer(desc);
        if (!next.IsValid())
        {
            Logger::Log::Error("Terrain pages: no page table ring of {} KiB; the terrain draws from its textures",
                               desc.size / 1024u);
            return {};
        }
        retired = m_Buffer;
        m_Buffer = next;
        m_SlotWords = slotWords;
        m_Written.fill(false);
    }
    if (!m_Written[slot] || !(m_Keys[slot] == key))
    {
        const size_t offset = static_cast<size_t>(slot) * m_SlotWords * sizeof(uint32);
        device.UpdateBuffer(m_Buffer, offset, heightWords.size_bytes(), heightWords.data());
        if (!surfaceWords.empty())
            device.UpdateBuffer(m_Buffer, offset + heightWords.size_bytes(), surfaceWords.size_bytes(),
                                surfaceWords.data());
        m_Keys[slot] = key;
        m_Written[slot] = true;
    }
    Slot out{};
    out.Buffer = m_Buffer;
    out.FirstWord = slot * m_SlotWords;
    out.Offset = static_cast<uint64>(out.FirstWord) * sizeof(uint32);
    out.Bytes = static_cast<uint64>(m_SlotWords) * sizeof(uint32);
    out.SurfaceBase = static_cast<uint32>(heightWords.size());
    return out;
}

void TerrainPageTableRing::Release(Rendering::IDevice* device)
{
    if (device != nullptr && m_Buffer.IsValid())
        device->DestroyBuffer(m_Buffer);
    m_Buffer = {};
    m_SlotWords = 0;
    m_Written.fill(false);
}

} // namespace GameEngine::TerrainECS
