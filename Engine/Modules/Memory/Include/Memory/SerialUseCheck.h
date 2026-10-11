#pragma once

#include <atomic>
#include <cassert>

namespace GameEngine::Memory::Detail
{

#if GE_DEBUG_INSTRUMENTATION
/// The Debug and DebugFast check that an externally serialized allocator has one user at a
/// time: each state change marks the word busy for its duration and asserts it was free. It
/// records no thread, so uses on different threads ordered by a join or a lock pass; two uses
/// that overlap in time assert.
class SerialUseCheck
{
  public:
    void Enter()
    {
        [[maybe_unused]] const bool wasBusy = m_Busy.exchange(true, std::memory_order_acquire);
        assert(!wasBusy && "two threads used this Arena at once; an Arena is externally serialized: one user at a "
                           "time (give each thread its own, or order the uses with a join or a lock)");
    }

    void Exit() { m_Busy.store(false, std::memory_order_release); }

  private:
    std::atomic<bool> m_Busy{false};
};
#endif

} // namespace GameEngine::Memory::Detail
