// NavigationSurface: what one surface bakes from (terrain, static collider shapes, mesh renderers),
// without a physics world, as mesh tiles or grid cells; how it reports its bake; how it is listed
// and saved.
#include "NavigationTestSurfaces.h"

#include "PathfindingECS/Components/NavigationAgent.h"
#include "PathfindingECS/Components/NavigationSurface.h"
#include "PathfindingECS/Navigation.h"
#include "PathfindingECS/NavigationService.h"
#include "PathfindingECS/NavigationSurfaceRuntime.h"
#include "PathfindingECS/Systems/NavigationWorldHooks.h"

#include "Components/Rendering/MeshRenderer.h"
#include "Components/Terrain/Terrain.h"
#include "Components/Transform.h"
#include "ECS/ComponentFactory.h"
#include "ECS/ComponentRegistry.h"
#include "ECS/ECSTemplates.h"
#include "ECS/World.h"
#include "Engine/Rendering/PrimitiveGenerator.h"
#include "Mathematics/Vector3.h"
#include "Pathfinding/DetourNavMap.h"
#include "Pathfinding/GridMap.h"
#include "Pathfinding/NavigationWorld.h"
#include "PhysicsECS/Components/BoxColliderShape.h"
#include "PhysicsECS/Components/PhysicsBody.h"
#include "PhysicsECS/Components/PhysicsCollider.h"
#include "PhysicsECS/PhysicsWorldService.h"
#include "Scene/SceneIO.h"
#include "TerrainECS/TerrainService.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

using namespace GameEngine;
using namespace GameEngine::PathfindingECS;

namespace
{

namespace TE = GameEngine::TerrainECS;
using Components::NavigationBakePhase;
using Components::NavigationSurface;
using Components::NavigationSurfaceMode;

constexpr float32 kDegreesToRadians = 3.14159265358979323846f / 180.0f;

Components::WorldTransform Placed(float32 x, float32 y, float32 z, float32 scaleX = 1.0f, float32 scaleY = 1.0f,
                                  float32 scaleZ = 1.0f)
{
    Components::WorldTransform transform{};
    transform.matrix[0] = scaleX;
    transform.matrix[5] = scaleY;
    transform.matrix[10] = scaleZ;
    transform.matrix[12] = x;
    transform.matrix[13] = y;
    transform.matrix[14] = z;
    transform.Version = 1u; // resolved, as TransformHierarchySystem leaves it
    return transform;
}

// Streams in one tile of a tiled terrain at full detail, as the terrain's streaming would.
void MakeTileResident(TE::TiledTerrainHandle terrain, TE::TileCoord coord)
{
    auto* tile = TE::TerrainService::Get().LoadTile(terrain, coord);
    ASSERT_NE(tile, nullptr);
    tile->LodState = TE::TileLodState::Full;
    tile->MarkFullDirty();
}

class NavigationSurfaceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // No PhysicsWorldService: the editor's edit mode has none, and the bake must not need one.
        ASSERT_EQ(PhysicsECS::PhysicsWorldService::TryGet(), nullptr);
        TE::TerrainService::Initialize();
        NavigationService::Initialize();
        world = std::make_unique<ECS::World>();
        RegisterNavigationWorldHooks(*world);
    }

    void TearDown() override
    {
        world.reset();
        ECS::ComponentRegistry::Clear();
        NavigationService::Shutdown();
        TE::TerrainService::Shutdown();
    }

    // A mesh surface over the box of half size `extent` around (0, 0, 0).
    ECS::Entity CreateMeshSurface(float32 extent, float32 cellSize = 0.5f)
    {
        auto entity = world->Create();
        NavigationSurface surface{};
        surface.CellSize = cellSize;
        surface.FitBounds = false;
        surface.Extent = {extent, 50.0f, extent};
        entity.Set(surface);
        world->ProcessCommands();
        return entity;
    }

    // A box collider on a body of `motionType`, `halfX` x `halfY` x `halfZ` metres about (x, y, z).
    ECS::Entity CreateBoxCollider(Physics::MotionType motionType, float32 x, float32 y, float32 z, float32 halfX,
                                  float32 halfY, float32 halfZ)
    {
        auto entity = world->Create();
        Components::PhysicsBody body{};
        body.motionType = motionType;
        entity.Set(body);
        entity.Set(Components::PhysicsCollider{});
        Components::BoxColliderShape box{};
        box.halfExtentsX = halfX;
        box.halfExtentsY = halfY;
        box.halfExtentsZ = halfZ;
        entity.Set(box);
        entity.Set(Components::Transform{});
        entity.Set(Placed(x, y, z));
        world->ProcessCommands();
        return entity;
    }

    // A flat terrain `size` metres square centred on the origin whose heights come from
    // `heightAt(x)` along X (world metres in, metres out), scaled by `heightScale`.
    template <typename HeightAt>
    ECS::Entity CreateTerrain(float32 size, float32 heightScale, uint32 samples, HeightAt heightAt)
    {
        Terrain::TerrainConfig config{};
        config.HeightmapWidth = config.HeightmapHeight = samples;
        config.WorldSizeX = config.WorldSizeZ = size;
        config.HeightScale = heightScale;
        config.LODLevels = 2;
        const auto handle = TE::TerrainService::Get().CreateTerrain(config);
        auto* data = TE::TerrainService::Get().GetTerrainData(handle);
        EXPECT_NE(data, nullptr);
        for (uint32 z = 0; z < samples; ++z)
            for (uint32 x = 0; x < samples; ++x)
            {
                const float32 worldX = -0.5f * size + size * static_cast<float32>(x) / static_cast<float32>(samples - 1);
                data->Heightfield.SetSample(x, z, heightAt(worldX) / heightScale);
            }
        data->MarkFullDirty();

        auto entity = world->Create();
        Components::Terrain terrain{};
        terrain.SizeX = terrain.SizeZ = size;
        terrain.HeightScale = heightScale;
        terrain.BaseSource = Components::TerrainBaseSource::Flat;
        terrain.TerrainDataHandle = handle.Index;
        terrain.TerrainDataGeneration = handle.Generation;
        entity.Set(terrain);
        entity.Set(Components::Transform{});
        entity.Set(Placed(0.0f, 0.0f, 0.0f));
        world->ProcessCommands();
        return entity;
    }

    // Updates NavigationSurfaceSystem until no surface is baking or waiting, at most `updates` times.
    void BakeAll(uint32 updates = 16)
    {
        NavigationSurfaceSystem system;
        for (uint32 i = 0; i < updates; ++i)
        {
            system.Update(*world, 1.0f / 60.0f);
            world->ProcessCommands();
            bool busy = false;
            world->Query<ECS::Read<NavigationSurface>>().Each([&](const NavigationSurface& surface) {
                busy = busy || surface.BakePhase == NavigationBakePhase::Baking ||
                       surface.BakePhase == NavigationBakePhase::Waiting;
            });
            if (!busy)
                return;
        }
    }

    Pathfinding::PathStatus FindPath(ECS::Entity& surfaceEntity, float32 fromX, float32 fromY, float32 fromZ,
                                     float32 toX, float32 toY, float32 toZ, float32* outEndY = nullptr)
    {
        auto& navigation = NavigationService::Get();
        Pathfinding::PathRequest request;
        request.StartX = fromX;
        request.StartY = fromY;
        request.StartZ = fromZ;
        request.EndX = toX;
        request.EndY = toY;
        request.EndZ = toZ;
        request.AgentRadius = 0.5f;
        request.AgentHeight = 1.8f;
        Pathfinding::PathHandle path;
        const auto status = navigation.FindPath(Testing::SurfaceMap(*world, surfaceEntity), request, path);
        if (outEndY && path.IsValid())
        {
            const uint32 count = navigation.GetPathBuffer().GetPointCount(path);
            if (count > 0)
                *outEndY = navigation.GetPathBuffer().GetPoint(path, count - 1).Y;
        }
        if (path.IsValid())
            navigation.GetPathBuffer().FreePath(path);
        return status;
    }

    const NavigationSurface& SurfaceOf(ECS::Entity& entity) { return *entity.Get<NavigationSurface>(); }

    std::unique_ptr<ECS::World> world;
};

} // namespace

// The editor's edit mode has no physics world, and neither does a test: the bake reads the
// collider shapes themselves.
TEST_F(NavigationSurfaceTest, BakeWithoutAPhysicsWorldReachesBaked)
{
    CreateBoxCollider(Physics::MotionType::Static, 0.0f, -0.5f, 0.0f, 20.0f, 0.5f, 20.0f);
    auto surface = CreateMeshSurface(16.0f);

    BakeAll();

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(surface));
    EXPECT_GT(SurfaceOf(surface).BakedWalkable, 0u);
    EXPECT_EQ(FindPath(surface, -12.0f, 0.0f, -12.0f, 12.0f, 0.0f, 12.0f), Pathfinding::PathStatus::Complete);
}

// A moving body is not part of the surface: only Static bodies' colliders are baked.
TEST_F(NavigationSurfaceTest, DynamicAndKinematicCollidersAreNotBaked)
{
    CreateBoxCollider(Physics::MotionType::Dynamic, 0.0f, -0.5f, 0.0f, 20.0f, 0.5f, 20.0f);
    CreateBoxCollider(Physics::MotionType::Kinematic, 0.0f, -0.5f, 0.0f, 20.0f, 0.5f, 20.0f);
    auto surface = CreateMeshSurface(16.0f);

    BakeAll();

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Failed);
    EXPECT_EQ(SurfaceOf(surface).BakeFailure, Components::NavigationBakeFailure::NoGeometry);
}

// The terrain is read from its CPU heightfield, the array physics builds its collider from.
TEST_F(NavigationSurfaceTest, TerrainIsBakedFromItsHeightfield)
{
    CreateTerrain(64.0f, 16.0f, 65, [](float32) { return 6.0f; });
    auto entity = world->Create();
    entity.Set(NavigationSurface{}); // fitted to the terrain
    world->ProcessCommands();

    BakeAll();

    ASSERT_EQ(SurfaceOf(entity).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(entity));
    float32 endY = 0.0f;
    EXPECT_EQ(FindPath(entity, -28.0f, 6.0f, -28.0f, 28.0f, 6.0f, 28.0f, &endY), Pathfinding::PathStatus::Complete);
    EXPECT_NEAR(endY, 6.0f, 0.3f);
}

// A terrain whose heightfield has not been built holds the bake, and the notice says so.
TEST_F(NavigationSurfaceTest, TerrainWithoutItsHeightfieldWaits)
{
    auto terrain = world->Create();
    Components::Terrain component{};
    component.SizeX = component.SizeZ = 64.0f;
    terrain.Set(component);
    terrain.Set(Components::Transform{});
    terrain.Set(Placed(0.0f, 0.0f, 0.0f));
    world->ProcessCommands();
    auto surface = CreateMeshSurface(16.0f);

    BakeAll(2);

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Waiting);
    EXPECT_EQ(Navigation::FormatBakeStatus(SurfaceOf(surface)), "Waiting for: the terrain's heightfield to be built");
}

// A terrain tile inside the bounds that has not streamed in holds the bake and is named; the bake
// completes once the tile is resident.
TEST_F(NavigationSurfaceTest, TiledTerrainWaitsForTheTileThatIsNotResident)
{
    TE::TiledTerrainConfig config{};
    config.WorldSizeX = 128.0f;
    config.WorldSizeZ = 64.0f; // square tiles of the shorter side: (0, 0) and (1, 0)
    config.HeightScale = 16.0f;
    auto& service = TE::TerrainService::Get();
    const auto handle = service.CreateTiledTerrain(config);
    const auto* data = service.GetTiledTerrainData(handle);
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->Config.TilesPerAxisX, 2u);
    ASSERT_EQ(data->Config.TilesPerAxisZ, 1u);
    MakeTileResident(handle, TE::TileCoord{0, 0});

    auto terrain = world->Create();
    Components::Terrain component{};
    component.SizeX = config.WorldSizeX;
    component.SizeZ = config.WorldSizeZ;
    component.HeightScale = config.HeightScale;
    component.TiledTerrainHandle = handle.Index;
    component.TiledTerrainGeneration = handle.Generation;
    terrain.Set(component);
    terrain.Set(Components::Transform{});
    terrain.Set(Placed(0.0f, 0.0f, 0.0f));
    auto surface = world->Create();
    NavigationSurface grid{}; // fitted to the terrain
    grid.Mode = NavigationSurfaceMode::Grid;
    grid.CellSize = 2.0f;
    surface.Set(grid);
    world->ProcessCommands();

    BakeAll(2);

    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Waiting) << Navigation::FormatBakeStatus(SurfaceOf(surface));
    EXPECT_EQ(Navigation::FormatBakeStatus(SurfaceOf(surface)), "Waiting for: terrain tile (1, 0) is not resident");

    MakeTileResident(handle, TE::TileCoord{1, 0});
    BakeAll();

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(surface));
}

// A trigger volume is not ground: a static trigger collider is left out of the bake.
TEST_F(NavigationSurfaceTest, TriggerCollidersAreNotBaked)
{
    auto trigger = CreateBoxCollider(Physics::MotionType::Static, 0.0f, -0.5f, 0.0f, 20.0f, 0.5f, 20.0f);
    world->GetComponentForWrite<Components::PhysicsCollider>(trigger.GetHandle())->isTrigger = true;
    world->ProcessCommands();
    auto surface = CreateMeshSurface(16.0f);

    BakeAll();

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Failed);
    EXPECT_EQ(SurfaceOf(surface).BakeFailure, Components::NavigationBakeFailure::NoGeometry);
}

// A bake with a new cell size replaces the mesh's Detour data: the crowd steering on the old data
// moves to the new data instead of keeping a pointer into freed memory.
TEST_F(NavigationSurfaceTest, RebakeWithANewCellSizeMovesTheCrowdToTheNewMesh)
{
    CreateBoxCollider(Physics::MotionType::Static, 0.0f, -0.5f, 0.0f, 20.0f, 0.5f, 20.0f);
    auto surface = CreateMeshSurface(16.0f);
    BakeAll();
    auto& crowd = NavigationService::Get().GetCrowdManager();
    const auto* mesh = ResolveNavigationSurfaceMesh(*world, SurfaceOf(surface));
    ASSERT_NE(mesh, nullptr);
    ASSERT_TRUE(crowd.IsBoundTo(*mesh));

    world->GetComponentForWrite<NavigationSurface>(surface.GetHandle())->CellSize = 0.25f;
    ASSERT_TRUE(Navigation::RequestBake(*world, surface.GetHandle()));
    BakeAll(64);

    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(surface));
    mesh = ResolveNavigationSurfaceMesh(*world, SurfaceOf(surface));
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->GetSettings().CellSize, 0.25f);
    EXPECT_TRUE(crowd.IsBoundTo(*mesh));
}

// A game sizing its own progress bar before a bake reads the count the bake makes, in both modes.
TEST_F(NavigationSurfaceTest, BakeUnitCountIsWhatTheBakeMakes)
{
    CreateBoxCollider(Physics::MotionType::Static, 0.0f, -0.5f, 0.0f, 40.0f, 0.5f, 40.0f);
    auto entity = world->Create();
    NavigationSurface authored{};
    authored.FitBounds = false;
    authored.Extent = {37.0f, 5.0f, 23.0f}; // 74 x 46 m: 3 x 2 mesh tiles, 74 x 46 grid cells
    entity.Set(authored);
    world->ProcessCommands();

    for (const auto [mode, cellSize] : {std::pair{NavigationSurfaceMode::Mesh, 0.5f}, std::pair{NavigationSurfaceMode::Grid, 1.0f}})
    {
        auto* surface = world->GetComponentForWrite<NavigationSurface>(entity.GetHandle());
        surface->Mode = mode;
        surface->CellSize = cellSize;
        ASSERT_TRUE(Navigation::RequestBake(*world, entity.GetHandle()));
        BakeAll(64);
        ASSERT_EQ(SurfaceOf(entity).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(entity));

        Mathematics::Vector3 min, max;
        ASSERT_TRUE(Navigation::SurfaceBounds(*world, entity.GetHandle(), min, max));
        EXPECT_EQ(Navigation::BakeUnitCount(SurfaceOf(entity), min, max), SurfaceOf(entity).BakeTotal)
            << (mode == NavigationSurfaceMode::Grid ? "grid" : "mesh");
    }
}

// A primitive has no model asset behind it; the bake generates its mesh from its GUID.
TEST_F(NavigationSurfaceTest, PrimitiveMeshRendererIsBaked)
{
    auto slab = world->Create();
    slab.Set(Engine::Renderer::PrimitiveGenerator::MakePrimitiveMeshRenderer(
        nullptr, Engine::Renderer::PrimitiveGenerator::CubeGuid(),
        Engine::Renderer::PrimitiveGenerator::DefaultMaterialGuid()));
    slab.Set(Components::Transform{});
    // The unit cube scaled to a 40 x 1 x 40 m slab whose top is at y = 2.
    slab.Set(Placed(0.0f, 1.5f, 0.0f, 40.0f, 1.0f, 40.0f));
    world->ProcessCommands();
    auto surface = CreateMeshSurface(16.0f);

    BakeAll();

    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(surface));
    float32 endY = 0.0f;
    EXPECT_EQ(FindPath(surface, -12.0f, 2.0f, -12.0f, 12.0f, 2.0f, 12.0f, &endY), Pathfinding::PathStatus::Complete);
    EXPECT_NEAR(endY, 2.0f, 0.3f);
}

// 32 m tiles at the default 0.5 m cell: a 100 m surface is 4 x 4 tiles, and a corner-to-corner
// path crosses six tile edges.
TEST_F(NavigationSurfaceTest, PathAcrossTileBordersIsComplete)
{
    Testing::CreateGroundPlane(*world);
    auto surface = CreateMeshSurface(50.0f);

    BakeAll();

    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked) << Navigation::FormatBakeStatus(SurfaceOf(surface));
    EXPECT_EQ(SurfaceOf(surface).BakeTotal, 16u);
    EXPECT_EQ(ResolveNavigationSurfaceMesh(*world, SurfaceOf(surface))->GetTileCount(), 16u);
    EXPECT_EQ(FindPath(surface, -45.0f, 0.0f, -45.0f, 45.0f, 0.0f, 45.0f), Pathfinding::PathStatus::Complete);
}

// At the defaults (0.5 m mesh cell, 1 m grid cell, 0.4 m step, 45 degrees) an agent walks up a 30
// degree slope and not a 50 degree one, on the mesh and on the grid: Max Slope governs slopes
// (#1930: the grid used to block slopes from about 12 degrees, and both kinds stopped at 21.8 degrees
// while the climb between cells was the step alone).
TEST_F(NavigationSurfaceTest, SlopeLimitHoldsOnMeshAndGrid)
{
    // x in [-32, 0): 30 degrees up; [0, 32]: 50 degrees up.
    const float32 moderate = std::tan(30.0f * kDegreesToRadians);
    const float32 steep = std::tan(50.0f * kDegreesToRadians);
    const auto heightAt = [=](float32 x) {
        return (std::min(x, 0.0f) + 32.0f) * moderate + std::max(x, 0.0f) * steep;
    };
    CreateTerrain(64.0f, 64.0f, 129, heightAt);
    auto mesh = world->Create();
    mesh.Set(NavigationSurface{});
    auto grid = world->Create();
    NavigationSurface gridSurface{};
    gridSurface.Mode = NavigationSurfaceMode::Grid;
    gridSurface.CellSize = 1.0f;
    grid.Set(gridSurface);
    world->ProcessCommands();

    BakeAll();

    ASSERT_EQ(SurfaceOf(mesh).BakePhase, NavigationBakePhase::Baked);
    ASSERT_EQ(SurfaceOf(grid).BakePhase, NavigationBakePhase::Baked);
    for (auto* surface : {&mesh, &grid})
    {
        const char* kind = surface == &mesh ? "mesh" : "grid";
        float32 endY = 0.0f;
        EXPECT_EQ(FindPath(*surface, -26.5f, heightAt(-26.5f), 0.5f, -4.5f, heightAt(-4.5f), 0.5f, &endY),
                  Pathfinding::PathStatus::Complete)
            << "up the 30 degree slope on the " << kind;
        EXPECT_NEAR(endY, heightAt(-4.5f), 0.6f) << kind;
        EXPECT_NE(FindPath(*surface, -26.5f, heightAt(-26.5f), 0.5f, 16.5f, heightAt(16.5f), 0.5f),
                  Pathfinding::PathStatus::Complete)
            << "up the 50 degree slope on the " << kind;
    }
}

// Max Slope does not turn a wall into a slope: a 1 m block is a step taller than an agent climbs,
// so its top is not reached from the ground.
TEST_F(NavigationSurfaceTest, StepHeightStillGovernsSteps)
{
    Testing::CreateGroundPlane(*world);
    CreateBoxCollider(Physics::MotionType::Static, 0.0f, 0.5f, 0.0f, 4.0f, 0.5f, 4.0f);
    auto surface = CreateMeshSurface(16.0f);

    BakeAll();

    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked);
    EXPECT_EQ(FindPath(surface, -12.0f, 0.0f, 0.0f, -8.0f, 0.0f, 0.0f), Pathfinding::PathStatus::Complete);
    EXPECT_NE(FindPath(surface, -12.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f), Pathfinding::PathStatus::Complete);
}

// The notice text the inspector shows, one line per phase.
TEST_F(NavigationSurfaceTest, BakeStatusReadsAsTheInspectorShowsIt)
{
    NavigationSurface surface{};
    EXPECT_EQ(Navigation::FormatBakeStatus(surface), "Not baked");

    surface.BakePhase = NavigationBakePhase::Waiting;
    surface.BlockingTileX = 3;
    surface.BlockingTileZ = 7;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface), "Waiting for: terrain tile (3, 7) is not resident");

    surface.BakePhase = NavigationBakePhase::Baking;
    surface.BakeDone = 212;
    surface.BakeTotal = 1024;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface), "Baking: 212 of 1,024 tiles");

    surface.BakePhase = NavigationBakePhase::Baked;
    surface.BakedWalkable = 1948;
    surface.BakedBytes = 512u * 1024u;
    surface.BakeVersion = 12;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface), "Baked: 1,024 tiles, 1,948 polygons, 0.5 MiB, version 12");
    surface.SettingsChanged = true;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface),
              "Out of date: the settings changed since the last bake (1,024 tiles). Bake to apply.");
    // Switching Mode before the next bake: the numbers count the mesh's tiles, and say so.
    surface.Mode = NavigationSurfaceMode::Grid;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface),
              "Out of date: the settings changed since the last bake (Mesh, 1,024 tiles). Bake to apply.");

    surface.BakePhase = NavigationBakePhase::Failed;
    surface.BakeFailure = Components::NavigationBakeFailure::NoGeometry;
    EXPECT_EQ(Navigation::FormatBakeStatus(surface),
              "Failed: no geometry inside the bounds; the bake reads the terrain, static collider shapes and mesh "
              "renderers");
}

// A baked surface that is asked to bake again builds from the current geometry and counts the bake.
TEST_F(NavigationSurfaceTest, RequestBakeBakesAgainAndCountsTheVersion)
{
    Testing::CreateGroundPlane(*world);
    auto surface = CreateMeshSurface(16.0f);
    BakeAll();
    ASSERT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked);
    const uint32 firstVersion = SurfaceOf(surface).BakeVersion;
    const auto firstMap = Testing::SurfaceMap(*world, surface);

    ASSERT_TRUE(Navigation::RequestBake(*world, surface.GetHandle()));
    BakeAll();

    EXPECT_EQ(SurfaceOf(surface).BakePhase, NavigationBakePhase::Baked);
    EXPECT_EQ(SurfaceOf(surface).BakeVersion, firstVersion + 1);
    EXPECT_EQ(Testing::SurfaceMap(*world, surface), firstMap) << "a re-bake must keep the handle agents are bound to";
}

// #3711: the component is offered in Add Component.
TEST_F(NavigationSurfaceTest, ComponentFactoryListsTheSurface)
{
    const auto types = ECS::ComponentFactory::RegisteredTypes();
    EXPECT_NE(std::find(types.begin(), types.end(), ECS::GetComponentTypeId<NavigationSurface>()), types.end());
}

// Every authored field survives a scene save and load; nothing the bake writes is saved.
TEST_F(NavigationSurfaceTest, SceneRoundTripKeepsTheAuthoredFields)
{
    auto entity = world->Create();
    NavigationSurface authored{};
    authored.Mode = NavigationSurfaceMode::Grid;
    authored.GridType = Pathfinding::GridType::Hex;
    authored.AgentRadius = 0.7f;
    authored.AgentHeight = 2.2f;
    authored.MaxSlope = 35.0f;
    authored.StepHeight = 0.3f;
    authored.CellSize = 1.5f;
    authored.FitBounds = false;
    authored.Center = {1.0f, 2.0f, 3.0f};
    authored.Extent = {40.0f, 5.0f, 30.0f};
    authored.BakeVersion = 9;
    entity.Set(authored);
    world->ProcessCommands();

    const auto path = std::filesystem::temp_directory_path() /
                      ("navigation-surface-roundtrip-" + std::to_string(world->GetWorldId()) + ".scene");
    struct FileCleanup
    {
        std::filesystem::path Path;
        ~FileCleanup()
        {
            std::error_code error;
            std::filesystem::remove(Path, error);
        }
    } cleanup{path};
    ASSERT_TRUE(Scene::SaveSceneToFile(*world, path));
    world->Clear();
    ASSERT_TRUE(Scene::LoadSceneFromFile(*world, path));

    ASSERT_EQ(world->Query<ECS::Read<NavigationSurface>>().Count(), 1u);
    world->Query<ECS::Read<NavigationSurface>>().Each([](const NavigationSurface& loaded) {
        EXPECT_EQ(loaded.Mode, NavigationSurfaceMode::Grid);
        EXPECT_EQ(loaded.GridType, Pathfinding::GridType::Hex);
        EXPECT_FLOAT_EQ(loaded.AgentRadius, 0.7f);
        EXPECT_FLOAT_EQ(loaded.AgentHeight, 2.2f);
        EXPECT_FLOAT_EQ(loaded.MaxSlope, 35.0f);
        EXPECT_FLOAT_EQ(loaded.StepHeight, 0.3f);
        EXPECT_FLOAT_EQ(loaded.CellSize, 1.5f);
        EXPECT_FALSE(loaded.FitBounds);
        EXPECT_FLOAT_EQ(loaded.Center.y, 2.0f);
        EXPECT_FLOAT_EQ(loaded.Extent.z, 30.0f);
        EXPECT_EQ(loaded.BakeVersion, 0u) << "bake state is runtime data and must not be saved";
    });
}
