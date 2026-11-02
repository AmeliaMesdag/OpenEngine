#pragma once

#include "Rendering/Core/Handle.h"
#include "Types/Types.h"

#include <array>
#include <span>

namespace GameEngine::Rendering
{
class IDevice;
} // namespace GameEngine::Rendering

namespace GameEngine::TerrainECS
{

/// One host-visible buffer with a slot per frame in flight. A slot holds the page tables of one terrain:
/// the height table, then the surface table. The terrain draw and the grass placement bind the same slot.
class TerrainPageTableRing
{
public:
    static constexpr uint32 kSlots = 4; // Rendering::IDevice::kMaxSupportedFramesInFlight

    /// What a slot holds. A slot is rewritten when any member differs: the generation catches a
    /// destroy-and-recreate whose table versions equal the slot's.
    struct Key
    {
        uint32 Terrain = 0;
        uint32 Generation = 0;
        uint64 HeightVersion = 0;
        uint64 SurfaceVersion = 0; // 0: no surface table
        bool operator==(const Key&) const = default;
    };

    /// Where a slot sits in the ring buffer.
    struct Slot
    {
        Rendering::BufferHandle Buffer;
        uint64 Offset = 0;    // bytes
        uint64 Bytes = 0;     // the slot's size
        uint32 FirstWord = 0; // Offset in words
        uint32 SurfaceBase = 0; // the first word of the surface table, relative to FirstWord
    };

    /// Writes the tables into slot `frameSlot` unless it holds `key`, and returns the slot. A table larger
    /// than a slot grows the ring: the previous buffer goes to `retired`, for the caller to destroy when no
    /// frame in flight reads it. Returns an invalid Buffer when the device refuses the allocation.
    Slot Bind(Rendering::IDevice& device, const Key& key, std::span<const uint32> heightWords,
              std::span<const uint32> surfaceWords, uint32 frameSlot, Rendering::BufferHandle& retired);
    /// Destroys the buffer (`device` non-null) or forgets it (a rebuilt device).
    void Release(Rendering::IDevice* device);

private:
    Rendering::BufferHandle m_Buffer;
    uint32 m_SlotWords = 0;
    std::array<Key, kSlots> m_Keys{};
    std::array<bool, kSlots> m_Written{};
};

} // namespace GameEngine::TerrainECS
