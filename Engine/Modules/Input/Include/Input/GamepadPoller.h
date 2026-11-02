#pragma once

namespace GameEngine::Input
{

struct GamepadFrame;

// Reads every gamepad's mapped state once per frame (slot 0 = first mapped controller).
// The first read (frame 0) reports no pad and does not query the platform: that query enumerates devices.
// A pad present at start arrives on the second read as a connection, the same way a hot plug does.
class GamepadPoller
{
  public:
    /// Fills `frame` with this frame's state. Main thread only: the platform's
    /// gamepad functions are not thread-safe.
    void Read(GamepadFrame& frame);

  private:
    bool m_FirstReadDone = false;
};

} // namespace GameEngine::Input
