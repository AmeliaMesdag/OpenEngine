#include <gtest/gtest.h>

#include "Scene/DefaultSceneEntities.h"

#include "Components/Rendering/ReflectionProbe.h"
#include "Components/Rendering/SkyEnvironment.h"
#include "Components/Rendering/SkySunPath.h"
#include "ECS/ECSTemplates.h"
#include "ECS/World.h"

using namespace GameEngine;

// A new scene takes its ambient from the sky. A reflection probe the camera stands in replaces the sky as the
// view's diffuse and specular environment (ReflectionProbeSystem), so a capture probe seeded into every new scene
// would define its global lighting from one capture point, and anything placed at that point would darken it all.
TEST(DefaultSceneEntities, ANewSceneTakesItsAmbientFromTheSky)
{
    ECS::World world;
    Editor::SeedDefaultSceneEntities(world, nullptr);

    int skies = 0;
    world.Query<ECS::Read<Components::SkyEnvironment>>().Each(
        [&](ECS::EntityHandle, const Components::SkyEnvironment&) { ++skies; });
    int probes = 0;
    world.Query<ECS::Read<Components::ReflectionProbe>>().Each(
        [&](ECS::EntityHandle, const Components::ReflectionProbe&) { ++probes; });

    EXPECT_EQ(skies, 1) << "positive control: the default scene seeds its sky";
    EXPECT_EQ(probes, 0) << "a new scene seeds no reflection probe; its ambient is the sky's";
}

// A new scene's sun stands mid-morning in the south-east, not overhead: about 42 degrees up at
// 10:00 at 40 degrees north on the equinox, so vertical faces are sunlit and shadows fall away
// from the default camera (0, 3, -10) looking +Z. Noon on the equator puts it at the zenith.
TEST(DefaultSceneEntities, ANewScenesSunStandsOffTheZenithInTheSouthEast)
{
    ECS::World world;
    Editor::SeedDefaultSceneEntities(world, nullptr);

    int skies = 0;
    world.Query<ECS::Read<Components::SkyEnvironment>>().Each(
        [&](ECS::EntityHandle, const Components::SkyEnvironment& sky)
        {
            ++skies;
            const Rendering::SolarFrame frame =
                Rendering::MakeSolarFrame(Components::SkySunPath::PathAngles(sky));
            const Rendering::SolarPosition sun = Rendering::SolarPositionAtHour(frame, sky.TimeOfDayHours);
            EXPECT_GT(sun.ElevationDegrees, 30.0f);
            EXPECT_LT(sun.ElevationDegrees, 55.0f);
            EXPECT_GT(sun.AzimuthDegrees, 90.0f) << "east of south";
            EXPECT_LT(sun.AzimuthDegrees, 180.0f) << "east of south";
        });
    EXPECT_EQ(skies, 1);
}
