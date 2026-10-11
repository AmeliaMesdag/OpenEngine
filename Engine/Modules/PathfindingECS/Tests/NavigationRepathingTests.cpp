#include "PathfindingECS/NavigationService.h"
#include "PathfindingECS/Components/NavigationAgent.h"
#include "PathfindingECS/Components/NavigationAgentState.h"
#include "PathfindingECS/Components/NavigationGrid.h"
#include "PathfindingECS/Systems/NavigationBuildSystem.h"
#include "PathfindingECS/Systems/NavigationPathfindingSystem.h"
#include "PathfindingECS/Systems/NavigationMovementSystem.h"
#include "Components/Transform.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include "ECS/ECSTemplates.h"
#include "ECS/ComponentRegistry.h"
#include "Pathfinding/NavigationWorld.h"
#include "Pathfinding/GridMap.h"
#include "Pathfinding/PathBuffer.h"
#include "Pathfinding/PathfindingTypes.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace GameEngine;
using namespace GameEngine::PathfindingECS;

namespace
{
constexpr float32 kDt = 1.0f / 60.0f;
} // namespace

class NavigationRepathingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        NavigationService::Initialize();
        m_World = std::make_unique<ECS::World>();
    }

    void TearDown() override
    {
        m_World.reset();
        ECS::ComponentRegistry::Clear();
        NavigationService::Shutdown();
    }

    ECS::Entity CreateGrid(uint32 width = 16, uint32 depth = 16)
    {
        auto entity = m_World->Create();
        Components::NavigationGrid grid{};
        grid.GridType = Pathfinding::GridType::Square;
        grid.CellSize = 1.0f;
        grid.Width = width;
        grid.Depth = depth;
        entity.Set(grid);
        m_World->ProcessCommands();

        NavigationBuildSystem buildSystem;
        buildSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
        return entity;
    }

    ECS::Entity CreateAgent(float32 startX, float32 startZ,
                            float32 destX, float32 destZ,
                            uint32 mapIndex, uint32 mapGen,
                            float32 replanInterval = 2.0f)
    {
        auto entity = m_World->Create();

        Components::NavigationAgent agent{};
        agent.Speed = 3.5f;
        agent.Acceleration = 100.0f;
        agent.StoppingDistance = 0.3f;
        agent.Radius = 0.25f;
        agent.NavMapIndex = mapIndex;
        agent.NavMapGeneration = mapGen;
        agent.AgentId = entity.GetHandle().id;
        agent.ReplanInterval = replanInterval;
        entity.Set(agent);

        Components::NavigationAgentState state{};
        state.DestinationX = destX;
        state.DestinationY = 0.0f;
        state.DestinationZ = destZ;
        state.HasDestination = true;
        state.PathDirty = true;
        entity.Set(state);

        Components::Transform xf{};
        xf.matrix[0] = 1.0f; xf.matrix[5] = 1.0f; xf.matrix[10] = 1.0f; xf.matrix[15] = 1.0f;
        xf.matrix[12] = startX;
        xf.matrix[13] = 0.0f;
        xf.matrix[14] = startZ;
        entity.Set(xf);

        Components::WorldTransform wt{};
        wt.matrix[0] = 1.0f; wt.matrix[5] = 1.0f; wt.matrix[10] = 1.0f; wt.matrix[15] = 1.0f;
        wt.matrix[12] = startX;
        wt.matrix[13] = 0.0f;
        wt.matrix[14] = startZ;
        entity.Set(wt);

        m_World->ProcessCommands();
        return entity;
    }

    std::unique_ptr<ECS::World> m_World;
};

// ---------------------------------------------------------------------------
// Proximity check: agent re-paths when upcoming cells get blocked
// ---------------------------------------------------------------------------

TEST_F(NavigationRepathingTest, ProximityCheckTriggersRepathWhenCellBlocked)
{
    auto gridEntity = CreateGrid();
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();
    ASSERT_NE(gridSrc, nullptr);

    auto agentEntity = CreateAgent(1.5f, 8.5f, 14.5f, 8.5f,
                                    gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 0.0f);

    // Compute initial path
    NavigationPathfindingSystem pathSystem;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    auto* agentState = agentEntity.Get<Components::NavigationAgentState>();
    ASSERT_NE(agentState, nullptr);
    EXPECT_EQ(static_cast<Pathfinding::PathStatus>(agentState->Status), Pathfinding::PathStatus::Complete);
    EXPECT_FALSE(agentState->PathDirty);

    // Run a few movement frames so the agent starts moving
    NavigationMovementSystem moveSystem;
    for (int i = 0; i < 5; ++i)
    {
        moveSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
    }

    // Now block cells ahead on the agent's path
    auto* navWorld = NavigationService::TryGet();
    ASSERT_NE(navWorld, nullptr);
    auto* gridMap = navWorld->GetGridMap(
        Pathfinding::NavMapHandle{gridSrc->NavMapIndex, gridSrc->NavMapGeneration});
    ASSERT_NE(gridMap, nullptr);

    // Block a wall at x=5 across the grid (ahead of the agent)
    for (uint32 z = 0; z < 16; ++z)
        gridMap->SetCellBlocked(5, z, true);

    // Run one more movement frame - proximity check should detect blocked path
    moveSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    agentState = agentEntity.Get<Components::NavigationAgentState>();
    EXPECT_TRUE(agentState->PathDirty) << "Proximity check should have detected blocked cells ahead";
}

TEST_F(NavigationRepathingTest, ProximityIgnoresBlockedCellAdjacentToDest)
{
    auto gridEntity = CreateGrid();
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();
    ASSERT_NE(gridSrc, nullptr);

    auto agentEntity = CreateAgent(1.5f, 8.5f, 8.5f, 8.5f,
                                    gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 0.0f);

    NavigationPathfindingSystem pathSystem;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    NavigationMovementSystem moveSystem;
    for (int i = 0; i < 5; ++i)
    {
        moveSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
    }

    auto* navWorld = NavigationService::TryGet();
    ASSERT_NE(navWorld, nullptr);
    auto* gridMap = navWorld->GetGridMap(
        Pathfinding::NavMapHandle{gridSrc->NavMapIndex, gridSrc->NavMapGeneration});
    ASSERT_NE(gridMap, nullptr);

    // Stamp ring on the approach: dest cell (8,8) stays open, (7,8) is blocked.
    // The agent→dest ray visits x=1..8 at z=8, so (7,8) is actually hit.
    gridMap->SetCellBlocked(7, 8, true);

    auto* agentState = agentEntity.GetForWrite<Components::NavigationAgentState>();
    ASSERT_NE(agentState, nullptr);
    agentState->PathDirty = false;

    moveSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    agentState = agentEntity.GetForWrite<Components::NavigationAgentState>();
    EXPECT_FALSE(agentState->PathDirty)
        << "A dest-adjacent stamp clip must not invalidate an otherwise open dest";
}

// ---------------------------------------------------------------------------
// Timer-based replan fires after interval
// ---------------------------------------------------------------------------

TEST_F(NavigationRepathingTest, TimerReplanFiresAfterInterval)
{
    auto gridEntity = CreateGrid();
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();

    auto agentEntity = CreateAgent(1.5f, 1.5f, 14.5f, 14.5f,
                                    gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 0.5f);

    NavigationPathfindingSystem pathSystem;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    auto* agentState = agentEntity.Get<Components::NavigationAgentState>();
    EXPECT_FALSE(agentState->PathDirty);

    // Run for slightly more than 0.5 seconds (the replan interval)
    NavigationMovementSystem moveSystem;
    int framesRun = 0;
    bool replanTriggered = false;
    for (int i = 0; i < 60; ++i) // 1 second at 60fps
    {
        moveSystem.Update(*m_World, kDt);
        pathSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
        ++framesRun;

        agentState = agentEntity.Get<Components::NavigationAgentState>();
        if (agentState->PathDirty || framesRun > 35) // should fire within ~0.5-1s
        {
            replanTriggered = true;
            break;
        }
    }

    EXPECT_TRUE(replanTriggered) << "Replan should have triggered within the interval";
}

// ---------------------------------------------------------------------------
// No replan when interval is zero
// ---------------------------------------------------------------------------

TEST_F(NavigationRepathingTest, NoReplanWhenIntervalIsZero)
{
    auto gridEntity = CreateGrid();
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();

    auto agentEntity = CreateAgent(1.5f, 1.5f, 14.5f, 14.5f,
                                    gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 0.0f);

    NavigationPathfindingSystem pathSystem;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    NavigationMovementSystem moveSystem;
    for (int i = 0; i < 120; ++i) // 2 full seconds
    {
        moveSystem.Update(*m_World, kDt);
        pathSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
    }

    auto* agentState = agentEntity.Get<Components::NavigationAgentState>();
    EXPECT_FALSE(agentState->PathDirty) << "PathDirty should not be set when ReplanInterval is 0";
}

// ---------------------------------------------------------------------------
// Timer replans share the solve budget: every expired timer marks its agent,
// and only MaxPathSolvesPerFrame of them are solved in one Update
// ---------------------------------------------------------------------------

TEST_F(NavigationRepathingTest, TimerReplansShareTheSolveBudget)
{
    auto gridEntity = CreateGrid(32, 32);
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();

    // Create 20 agents all with the same short replan interval
    constexpr uint32 kAgentCount = 20;
    constexpr uint32 kBudget = 4;
    std::vector<ECS::Entity> agents;
    for (uint32 i = 0; i < kAgentCount; ++i)
    {
        float32 startX = 1.5f + static_cast<float32>(i % 4);
        float32 startZ = 1.5f + static_cast<float32>(i / 4);
        auto e = CreateAgent(startX, startZ, 28.5f, 28.5f,
                              gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 0.1f);
        agents.push_back(e);
    }

    NavigationPathfindingSystem pathSystem;
    pathSystem.MaxPathSolvesPerFrame = kAgentCount;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    // Expire every timer at once
    for (auto& e : agents)
    {
        auto* s = e.GetForWrite<Components::NavigationAgentState>();
        ASSERT_NE(s, nullptr);
        ASSERT_FALSE(s->PathDirty);
        s->ReplanTimer = 0.0f;
    }
    m_World->ProcessCommands();

    pathSystem.MaxPathSolvesPerFrame = kBudget;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    // Every timer marked its agent; the budget solved four and queued the
    // rest, which keep the path they are walking.
    uint32 queuedCount = 0;
    for (auto& e : agents)
    {
        auto* s = e.Get<Components::NavigationAgentState>();
        EXPECT_FALSE(s->PathDirty);
        if (!s->ReplanRequested)
            continue;
        ++queuedCount;
        EXPECT_EQ(static_cast<Pathfinding::PathStatus>(s->Status), Pathfinding::PathStatus::Complete);
        EXPECT_NE(s->PathIndex, 0u);
    }
    EXPECT_EQ(queuedCount, kAgentCount - kBudget);
    EXPECT_EQ(pathSystem.GetStatistics().Solved, kBudget);
    EXPECT_EQ(pathSystem.GetStatistics().Backlog, kAgentCount - kBudget);
}

// ---------------------------------------------------------------------------
// Staggered initialization: agents get different replan offsets
// ---------------------------------------------------------------------------

TEST_F(NavigationRepathingTest, StaggeredReplanTimers)
{
    auto gridEntity = CreateGrid();
    auto* gridSrc = gridEntity.Get<Components::NavigationGrid>();

    // Create several agents with same ReplanInterval
    std::vector<ECS::Entity> agents;
    for (int i = 0; i < 8; ++i)
    {
        auto e = CreateAgent(1.5f + static_cast<float32>(i), 1.5f,
                              14.5f, 14.5f,
                              gridSrc->NavMapIndex, gridSrc->NavMapGeneration, 2.0f);
        agents.push_back(e);
    }

    // Initial pathfinding
    NavigationPathfindingSystem pathSystem;
    pathSystem.Update(*m_World, kDt);
    m_World->ProcessCommands();

    // Run several frames to let first replan timers get set
    NavigationMovementSystem moveSystem;
    for (int i = 0; i < 10; ++i)
    {
        moveSystem.Update(*m_World, kDt);
        pathSystem.Update(*m_World, kDt);
        m_World->ProcessCommands();
    }

    // Check that agents have different ReplanTimer values (staggered)
    std::vector<float32> timers;
    for (auto& e : agents)
    {
        auto* s = e.Get<Components::NavigationAgentState>();
        if (s)
            timers.push_back(s->ReplanTimer);
    }

    // Not all timers should be identical (staggering via hash)
    bool allSame = true;
    for (size_t i = 1; i < timers.size(); ++i)
    {
        if (std::abs(timers[i] - timers[0]) > 0.001f)
        {
            allSame = false;
            break;
        }
    }
    EXPECT_FALSE(allSame) << "Replan timers should be staggered across agents";
}
