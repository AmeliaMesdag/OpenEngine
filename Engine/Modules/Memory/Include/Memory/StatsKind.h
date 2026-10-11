#pragma once

#include <cstdint>

namespace GameEngine::Memory
{

/// What registered a stats record, and so which of its counts are meaningful.
enum class StatsKind : std::uint8_t
{
    /// A `TrackedBytes`: `Bytes` is what its owner reported; the arena counts are 0.
    TrackedBytes,
    /// An `Arena`: `Bytes` is the bytes in use, beside the reserved bytes, the high-water
    /// mark and the block count.
    Arena,
};

} // namespace GameEngine::Memory
