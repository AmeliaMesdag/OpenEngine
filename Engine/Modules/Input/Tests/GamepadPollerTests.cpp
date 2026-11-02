// The gamepad poller keeps the platform's device enumeration off frame 0.
// The suite never initializes GLFW, so each GLFW joystick call reports GLFW_NOT_INITIALIZED
// through the error callback: a read that queries the platform leaves a count.
#include "Input/GamepadFrame.h"
#include "Input/GamepadPoller.h"

#include <GLFW/glfw3.h>
#include <gtest/gtest.h>

using namespace GameEngine::Input;

namespace
{

int g_PlatformCalls = 0;

void CountPlatformCall(int, const char*)
{
    ++g_PlatformCalls;
}

class GamepadPollerTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        g_PlatformCalls = 0;
        m_PreviousCallback = glfwSetErrorCallback(&CountPlatformCall);
    }

    void TearDown() override { glfwSetErrorCallback(m_PreviousCallback); }

  private:
    GLFWerrorfun m_PreviousCallback = nullptr;
};

bool AnySlotConnected(const GamepadFrame& frame)
{
    for (const GamepadFrame::Slot& slot : frame.slots)
    {
        if (slot.connected)
            return true;
    }
    return false;
}

} // namespace

TEST_F(GamepadPollerTests, FirstReadReportsNoPadAndLeavesTheEnumerationToTheSecondRead)
{
    GamepadPoller poller;
    GamepadFrame frame;
    frame.slots[0].connected = true;

    poller.Read(frame);
    EXPECT_EQ(g_PlatformCalls, 0) << "frame 0's read must not call the platform: its first call enumerates devices";
    EXPECT_FALSE(AnySlotConnected(frame)) << "the first read reports every slot empty";

    poller.Read(frame);
    EXPECT_GT(g_PlatformCalls, 0) << "the second read queries the platform; a zero here means the instrument is dead";
}
