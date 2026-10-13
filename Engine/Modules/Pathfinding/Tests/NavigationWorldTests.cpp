#include <gtest/gtest.h>
#include "Pathfinding/NavigationWorld.h"
#include "JobSystem/WorkStealingThreadPool.h"

#include <vector>

using namespace GameEngine;
using namespace GameEngine::Pathfinding;

TEST(NavigationWorldTests, AddRemoveGridMap)
{
    NavigationWorld world;
    GridSettings settings;
    settings.Width = 10;
    settings.Depth = 10;
    NavMapHandle handle = world.AddGridMap(settings);
    ASSERT_TRUE(handle.IsValid());
    EXPECT_NE(world.GetGridMap(handle), nullptr);
    EXPECT_NE(world.GetMap(handle), nullptr);

    world.RemoveMap(handle);
    EXPECT_EQ(world.GetGridMap(handle), nullptr);
    EXPECT_EQ(world.GetMap(handle), nullptr);
}

TEST(NavigationWorldTests, AddRemoveDetourNavMap)
{
    NavigationWorld world;
    NavMapHandle handle = world.AddDetourNavMap();
    ASSERT_TRUE(handle.IsValid());
    EXPECT_NE(world.GetDetourNavMap(handle), nullptr);

    world.RemoveMap(handle);
    EXPECT_EQ(world.GetDetourNavMap(handle), nullptr);
}

TEST(NavigationWorldTests, FindPathSync)
{
    NavigationWorld world;
    GridSettings settings;
    settings.Width = 10;
    settings.Depth = 10;
    settings.CellSize = 1.0f;
    NavMapHandle handle = world.AddGridMap(settings);

    PathRequest request;
    request.StartX = 0.5f; request.StartZ = 0.5f;
    request.EndX = 9.5f; request.EndZ = 9.5f;

    PathHandle outPath;
    PathStatus status = world.FindPath(handle, request, outPath);
    EXPECT_EQ(status, PathStatus::Complete);
    EXPECT_TRUE(outPath.IsValid());
    EXPECT_GT(world.GetPathBuffer().GetPointCount(outPath), 0u);
}

TEST(NavigationWorldTests, NavLinkBasic)
{
    NavigationWorld world;
    GridSettings s1, s2;
    s1.Width = 5; s1.Depth = 5; s1.CellSize = 1.0f;
    s2.Width = 5; s2.Depth = 5; s2.CellSize = 1.0f; s2.OriginX = 5.0f;

    NavMapHandle h1 = world.AddGridMap(s1);
    NavMapHandle h2 = world.AddGridMap(s2);

    NavLink link;
    link.SourceMap = h1;
    link.SourceX = 4.5f; link.SourceZ = 2.5f;
    link.TargetMap = h2;
    link.TargetX = 5.5f; link.TargetZ = 2.5f;
    link.Bidirectional = true;

    NavLinkId id = world.AddNavLink(link);
    EXPECT_TRUE(id.IsValid());

    world.RemoveNavLink(id);
    // After removal, the link slot should be freed (no crash on removal)
}

TEST(NavigationWorldTests, MultipleMapSlotReuse)
{
    NavigationWorld world;
    GridSettings settings;
    settings.Width = 5;
    settings.Depth = 5;

    NavMapHandle h1 = world.AddGridMap(settings);
    NavMapHandle h2 = world.AddGridMap(settings);
    world.RemoveMap(h1);

    NavMapHandle h3 = world.AddGridMap(settings);
    // h3 should reuse h1's slot
    EXPECT_EQ(h3.Index, h1.Index);
    // But different generation
    EXPECT_NE(h3.Generation, h1.Generation);
}

class NavigationWorldTest : public ::testing::Test
{
protected:
    NavigationWorld m_World;
};

TEST_F(NavigationWorldTest, GetNavLinkReturnsNullForInvalidId)
{
    NavLinkId invalidId;
    EXPECT_EQ(m_World.GetNavLink(invalidId), nullptr);

    NavLinkId outOfRange{999, 1};
    EXPECT_EQ(m_World.GetNavLink(outOfRange), nullptr);
}

TEST_F(NavigationWorldTest, ForEachNavLinkVisitsAllActiveLinks)
{
    GridSettings s1;
    s1.Width = 5; s1.Depth = 5; s1.CellSize = 1.0f;
    GridSettings s2 = s1;
    s2.OriginX = 5.0f;

    NavMapHandle h1 = m_World.AddGridMap(s1);
    NavMapHandle h2 = m_World.AddGridMap(s2);

    NavLink linkA;
    linkA.SourceMap = h1; linkA.SourceX = 4.5f; linkA.SourceZ = 1.5f;
    linkA.TargetMap = h2; linkA.TargetX = 5.5f; linkA.TargetZ = 1.5f;
    linkA.Bidirectional = true;

    NavLink linkB;
    linkB.SourceMap = h1; linkB.SourceX = 4.5f; linkB.SourceZ = 3.5f;
    linkB.TargetMap = h2; linkB.TargetX = 5.5f; linkB.TargetZ = 3.5f;
    linkB.Bidirectional = false;

    NavLinkId idA = m_World.AddNavLink(linkA);
    NavLinkId idB = m_World.AddNavLink(linkB);

    EXPECT_EQ(m_World.GetActiveNavLinkCount(), 2u);

    uint32 visitCount = 0;
    m_World.ForEachNavLink([&](NavLinkId /*id*/, const NavLink& /*link*/)
    {
        ++visitCount;
        return true;
    });
    EXPECT_EQ(visitCount, 2u);

    // Remove one and verify count drops
    m_World.RemoveNavLink(idA);
    EXPECT_EQ(m_World.GetActiveNavLinkCount(), 1u);

    visitCount = 0;
    m_World.ForEachNavLink([&](NavLinkId /*id*/, const NavLink& /*link*/)
    {
        ++visitCount;
        return true;
    });
    EXPECT_EQ(visitCount, 1u);
}

TEST_F(NavigationWorldTest, CrossMapPathfindingViaNavLinks)
{
    // Grid A: origin (0,0), 8x8
    GridSettings settingsA;
    settingsA.Width = 8;
    settingsA.Depth = 8;
    settingsA.CellSize = 1.0f;
    settingsA.OriginX = 0.0f;
    settingsA.OriginZ = 0.0f;

    // Grid B: origin (8,0), 8x8
    GridSettings settingsB;
    settingsB.Width = 8;
    settingsB.Depth = 8;
    settingsB.CellSize = 1.0f;
    settingsB.OriginX = 8.0f;
    settingsB.OriginZ = 0.0f;

    NavMapHandle mapA = m_World.AddGridMap(settingsA);
    NavMapHandle mapB = m_World.AddGridMap(settingsB);
    ASSERT_TRUE(mapA.IsValid());
    ASSERT_TRUE(mapB.IsValid());

    // Add bidirectional NavLink connecting A's right edge to B's left edge
    NavLink link;
    link.SourceMap = mapA;
    link.SourceX = 7.5f; link.SourceY = 0.0f; link.SourceZ = 4.5f;
    link.TargetMap = mapB;
    link.TargetX = 8.5f; link.TargetY = 0.0f; link.TargetZ = 4.5f;
    link.TraversalCost = 1.0f;
    link.Bidirectional = true;

    NavLinkId linkId = m_World.AddNavLink(link);
    ASSERT_TRUE(linkId.IsValid());

    // Verify GetNavLink returns the correct link
    const NavLink* retrieved = m_World.GetNavLink(linkId);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->SourceMap, mapA);
    EXPECT_EQ(retrieved->TargetMap, mapB);
    EXPECT_FLOAT_EQ(retrieved->SourceX, 7.5f);
    EXPECT_FLOAT_EQ(retrieved->TargetX, 8.5f);
    EXPECT_TRUE(retrieved->Bidirectional);

    // Verify ForEachNavLink enumerates the link
    uint32 enumCount = 0;
    m_World.ForEachNavLink([&](NavLinkId id, const NavLink& /*visited*/)
    {
        EXPECT_EQ(id.Index, linkId.Index);
        EXPECT_EQ(id.Generation, linkId.Generation);
        ++enumCount;
        return true;
    });
    EXPECT_EQ(enumCount, 1u);

    // FindPath within grid A works
    PathRequest reqA;
    reqA.StartX = 1.5f; reqA.StartY = 0.0f; reqA.StartZ = 4.5f;
    reqA.EndX = 7.5f; reqA.EndY = 0.0f; reqA.EndZ = 4.5f;
    PathHandle pathA;
    PathStatus statusA = m_World.FindPath(mapA, reqA, pathA);
    EXPECT_EQ(statusA, PathStatus::Complete);
    EXPECT_TRUE(pathA.IsValid());

    // FindPath within grid B works
    PathRequest reqB;
    reqB.StartX = 8.5f; reqB.StartY = 0.0f; reqB.StartZ = 4.5f;
    reqB.EndX = 14.5f; reqB.EndY = 0.0f; reqB.EndZ = 4.5f;
    PathHandle pathB;
    PathStatus statusB = m_World.FindPath(mapB, reqB, pathB);
    EXPECT_EQ(statusB, PathStatus::Complete);
    EXPECT_TRUE(pathB.IsValid());

    // Verify the link conceptually connects the two maps
    EXPECT_EQ(retrieved->SourceMap, mapA);
    EXPECT_EQ(retrieved->TargetMap, mapB);
}

TEST_F(NavigationWorldTest, FindPathAcrossLinksJoinsTheSegments)
{
    GridSettings settingsA;
    settingsA.Width = 8; settingsA.Depth = 8; settingsA.CellSize = 1.0f;
    GridSettings settingsB = settingsA;
    settingsB.OriginX = 8.0f;
    const NavMapHandle mapA = m_World.AddGridMap(settingsA);
    const NavMapHandle mapB = m_World.AddGridMap(settingsB);

    NavLink link;
    link.SourceMap = mapA;
    link.SourceX = 7.5f; link.SourceZ = 4.5f;
    link.TargetMap = mapB;
    link.TargetX = 8.5f; link.TargetZ = 4.5f;
    const NavLinkId linkId = m_World.AddNavLink(link);
    ASSERT_TRUE(linkId.IsValid());

    PathRequest request;
    request.StartX = 1.5f; request.StartZ = 4.5f;
    request.EndX = 14.5f; request.EndZ = 4.5f;
    PathHandle path;
    ASSERT_EQ(m_World.FindPathAcrossLinks(request, path), PathStatus::Complete);
    const PathBuffer& buffer = m_World.GetPathBuffer();
    const uint32 count = buffer.GetPointCount(path);
    ASSERT_GE(count, 2u);
    EXPECT_FLOAT_EQ(buffer.GetPoint(path, 0).X, 1.5f);
    EXPECT_FLOAT_EQ(buffer.GetPoint(path, count - 1).X, 14.5f);
    // Map A's segment ends at the link's source; map B's starts at its target
    // and drops that first point when the segments are joined.
    bool reachesTheLink = false;
    for (uint32 i = 0; i < count; ++i)
        reachesTheLink = reachesTheLink || buffer.GetPoint(path, i).X == 7.5f;
    EXPECT_TRUE(reachesTheLink) << "the path does not leave map A at the link's source";

    m_World.RemoveNavLink(linkId);
    PathHandle unlinked;
    EXPECT_EQ(m_World.FindPathAcrossLinks(request, unlinked), PathStatus::Failed);
}

TEST_F(NavigationWorldTest, AutoGenerateBoundaryLinksCreatesLinks)
{
    GridSettings settingsA;
    settingsA.Width = 4; settingsA.Depth = 4; settingsA.CellSize = 1.0f;
    settingsA.OriginX = 0.0f; settingsA.OriginZ = 0.0f;

    GridSettings settingsB;
    settingsB.Width = 4; settingsB.Depth = 4; settingsB.CellSize = 1.0f;
    settingsB.OriginX = 3.0f; settingsB.OriginZ = 0.0f; // B overlaps A's last column — boundary cell centers coincide at X=3.5

    NavMapHandle mapA = m_World.AddGridMap(settingsA);
    NavMapHandle mapB = m_World.AddGridMap(settingsB);
    ASSERT_TRUE(mapA.IsValid());
    ASSERT_TRUE(mapB.IsValid());

    EXPECT_EQ(m_World.GetActiveNavLinkCount(), 0u);
    m_World.AutoGenerateBoundaryLinks(mapA, mapB);

    EXPECT_GE(m_World.GetActiveNavLinkCount(), 1u);

    bool foundValidLink = false;
    m_World.ForEachNavLink([&](NavLinkId /*id*/, const NavLink& link)
    {
        EXPECT_TRUE(link.Bidirectional);
        foundValidLink = true;
        return true;
    });
    EXPECT_TRUE(foundValidLink);
}

// SolveGridPaths solves every request before it returns, on a pool or on the
// caller, writes each solve's points without touching the PathBuffer, and
// leaves HasActivePathJobs false.
TEST(NavigationWorldTests, SolveGridPathsMatchesFindPathAndLeavesNoActiveJobs)
{
    NavigationWorld world;
    GridSettings settings;
    settings.Width = 24; settings.Depth = 24; settings.CellSize = 1.0f;
    const NavMapHandle handle = world.AddGridMap(settings);
    for (uint32 z = 2; z < 22; ++z)
        world.GetGridMap(handle)->SetCellBlocked(12, z, true);

    std::vector<GridPathSolve> solves(6);
    for (uint32 i = 0; i < solves.size(); ++i)
    {
        solves[i].Map = handle;
        solves[i].Request.StartX = 1.5f; solves[i].Request.StartZ = 1.5f + 4.0f * static_cast<float32>(i);
        solves[i].Request.EndX = 22.5f; solves[i].Request.EndZ = 22.5f - 4.0f * static_cast<float32>(i);
    }
    solves.back().Map = NavMapHandle{};

    JobSystem::WorkStealingThreadPool pool(4);
    for (JobSystem::WorkStealingThreadPool* poolArm : {static_cast<JobSystem::WorkStealingThreadPool*>(nullptr), &pool})
    {
        EXPECT_FALSE(world.HasActivePathJobs());
        world.SolveGridPaths(poolArm, solves);
        EXPECT_FALSE(world.HasActivePathJobs());
        EXPECT_EQ(world.GetPathBuffer().GetPointCount({0, 1}), 0u) << "a solve allocated in the PathBuffer";
        for (uint32 i = 0; i + 1 < solves.size(); ++i)
        {
            PathHandle expected;
            ASSERT_EQ(world.FindPath(handle, solves[i].Request, expected), PathStatus::Complete);
            ASSERT_EQ(solves[i].Status, PathStatus::Complete) << "solve " << i;
            std::vector<PathPoint> points(world.GetPathBuffer().GetPointCount(expected));
            world.GetPathBuffer().CopyPoints(expected, points.data(), static_cast<uint32>(points.size()));
            world.GetPathBuffer().FreePath(expected);
            ASSERT_EQ(solves[i].Points.size(), points.size()) << "solve " << i;
            for (std::size_t p = 0; p < points.size(); ++p)
            {
                EXPECT_EQ(solves[i].Points[p].X, points[p].X);
                EXPECT_EQ(solves[i].Points[p].Z, points[p].Z);
            }
        }
        EXPECT_EQ(solves.back().Status, PathStatus::Failed) << "a handle naming no grid map solves to Failed";
        EXPECT_TRUE(solves.back().Points.empty());
    }
    pool.Shutdown();
}

TEST(NavigationWorldTests, MapHandlesNeverAliasAcrossInstances)
{
    GridSettings settings;
    settings.Width = 4;
    settings.Depth = 4;
    NavMapHandle stale;
    {
        NavigationWorld first;
        stale = first.AddGridMap(settings);
    }
    NavigationWorld second;
    const NavMapHandle fresh = second.AddGridMap(settings);
    EXPECT_EQ(fresh.Index, stale.Index);
    EXPECT_NE(fresh.Generation, stale.Generation);
    EXPECT_EQ(second.GetGridMap(stale), nullptr);
    EXPECT_NE(second.GetGridMap(fresh), nullptr);
}
