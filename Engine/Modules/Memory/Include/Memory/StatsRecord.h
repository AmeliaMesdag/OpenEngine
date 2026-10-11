#pragma once

#include "Memory/Residency.h"
#include "Memory/StatsKind.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace GameEngine::Memory::Detail
{

/// One entry of the process's stats registry, stored inside the object it describes
/// (a `TrackedBytes` or an `Arena`) and linked into the registry for that object's life.
///
/// The owner writes the counts and the destructor bounds with relaxed atomics, except the
/// release store of `DestructorLow` that publishes or clears an interval, and never takes a
/// lock; the registry reads them under its mutex at a snapshot and at the image-pin
/// query. `Name`, `Kind` and `Residency` are written before the record is linked, and
/// published by the registry mutex; the links are written only under that mutex, at
/// registration and unregistration. A `TrackedBytes`, written on every counted
/// allocation from any thread, keeps its record in a `FalseSharingPadded`; an `Arena` writes
/// its record only at block acquisition, `Rewind` and `Reset`, and keeps it unpadded.
struct StatsRecord
{
    /// Characters a name keeps; a longer name is truncated (and asserted in Debug and DebugFast).
    static constexpr std::size_t kMaxNameLength = 47;

    /// A copy of the owner's name, NUL-terminated: it stays readable when the string the
    /// owner was constructed from is gone, or lived in an image that has since unloaded.
    char Name[kMaxNameLength + 1] = {};
    StatsKind Kind = StatsKind::TrackedBytes;
    Memory::Residency Residency = Memory::Residency::Cpu;
    /// A `TrackedBytes`' count; an arena's bytes in use.
    std::atomic<std::uint64_t> Bytes{0};
    /// An arena's block bytes, its largest `Bytes` so far, and its block count.
    std::atomic<std::uint64_t> ReservedBytes{0};
    std::atomic<std::uint64_t> HighWaterBytes{0};
    std::atomic<std::uint64_t> Blocks{0};
    /// The lowest and highest address of a destructor an arena has recorded and not yet run,
    /// both 0 when it holds none: what `DescribeArenasPinningImage` tests against an image.
    /// `DestructorLow` is the "holds none" sentinel: stored after `DestructorHigh` with
    /// release, loaded by the query with acquire.
    std::atomic<std::uint64_t> DestructorLow{0};
    std::atomic<std::uint64_t> DestructorHigh{0};
    StatsRecord* Previous = nullptr;
    StatsRecord* Next = nullptr;
};

} // namespace GameEngine::Memory::Detail
