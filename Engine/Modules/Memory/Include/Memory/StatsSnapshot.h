#pragma once

#include "Memory/Residency.h"
#include "Memory/StatsKind.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine::Memory
{

/// One registered stats record, copied out of the registry.
struct StatsEntry
{
    std::string Name;
    StatsKind Kind = StatsKind::TrackedBytes;
    Memory::Residency Residency = Memory::Residency::Cpu;
    /// A `TrackedBytes`' count; an arena's bytes in use.
    std::uint64_t Bytes = 0;
    /// An arena's block bytes, its largest bytes in use so far, and its block count; 0 for a
    /// `TrackedBytes`.
    std::uint64_t ReservedBytes = 0;
    std::uint64_t HighWaterBytes = 0;
    std::uint64_t Blocks = 0;
};

/// Copies every record registered at the moment of the call (every live `TrackedBytes` and
/// `Arena`), newest first. Takes the registry mutex; meant for tools, tests and debug views,
/// not for a per-frame path.
std::vector<StatsEntry> SnapshotStats();

} // namespace GameEngine::Memory
