// The Scene View's exposure modes: Scene Light's incident-meter derivation (SceneLightEv100 and
// the world reading behind it), the mode's persistence, and the handoff into Fixed.

#include <gtest/gtest.h>

#include "Components/Rendering/Light.h"
#include "Components/Rendering/PostProcessVolume.h"
#include "Components/Rendering/SkyEnvironment.h"
#include "Components/Rendering/SkySunPath.h"
#include "Components/Rendering/LightPhotometry.h"
#include "Editor/Settings/SceneViewSettings.h"
#include "Editor/Settings/SettingsStore.h"
#include "Engine/Rendering/RenderServices.h"
#include "SceneView/SceneLightExposure.h"
#include "TestEnvVar.h"
#include "TestTempDir.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

using namespace GameEngine;
using Editor::SceneLightEv100;
using Editor::SceneLightIncidence;
using Editor::SceneViewExposureMode;
using Editor::SceneViewSettings;

namespace
{
constexpr float kOverheadSunLux = 100000.0f;

float DegreesSine(float degrees)
{
    return std::sin(degrees * std::numbers::pi_v<float> / 180.0f);
}

// The directional lights a world submits, as the renderer finalizes them: each Finalize submits
// them to a fresh RenderServices and returns its strongest-first list, the span the Scene View
// meters.
class FinalizedLights
{
  public:
    // A white directional light of `lux` shining down at `elevationDegrees` below the horizon toward
    // +Z (directionWS is the shine direction, as extraction leaves it). Returns its index for At.
    size_t Add(float lux, float elevationDegrees)
    {
        Engine::Renderer::ExtractedLight light{};
        light.type = Components::LightType::Directional;
        light.intensity = Components::LightIntensityToUnitless(lux, Components::LightUnit::Lux);
        light.SortId = static_cast<uint32_t>(m_Submitted.size() + 1);
        Aim(light, elevationDegrees);
        m_Submitted.push_back(light);
        return m_Submitted.size() - 1;
    }
    Engine::Renderer::ExtractedLight& At(size_t index) { return m_Submitted[index]; }
    static void Aim(Engine::Renderer::ExtractedLight& light, float elevationDegrees)
    {
        const float radians = elevationDegrees * std::numbers::pi_v<float> / 180.0f;
        light.directionWS[0] = 0.0f;
        light.directionWS[1] = -std::sin(radians);
        light.directionWS[2] = std::cos(radians);
    }
    std::span<const Engine::Renderer::ExtractedLight> Finalize()
    {
        m_Services = std::make_unique<Engine::Renderer::RenderServices>();
        for (const Engine::Renderer::ExtractedLight& light : m_Submitted)
            m_Services->SubmitLight(0u, light);
        m_Services->FinalizeWorldLights(0u);
        return m_Services->GetWorldLights(0u);
    }

  private:
    std::vector<Engine::Renderer::ExtractedLight> m_Submitted;
    std::unique_ptr<Engine::Renderer::RenderServices> m_Services;
};
} // namespace

// An overhead 100 000 lx sun reads EV100 15.3, the incident reading for a clear noon.
TEST(SceneLightExposure, AnOverheadSunReadsItsFullIlluminance)
{
    const SceneLightIncidence zenith{.DirectLux = kOverheadSunLux};
    EXPECT_NEAR(SceneLightEv100(zenith), std::log2(kOverheadSunLux / 2.5f), 1e-4f);
}

// 10:00 at 40 degrees north on the equinox: the sun stands about 42 degrees up and a horizontal
// surface receives its lux times the sine of that elevation.
TEST(SceneLightExposure, AMorningSunReadsTheSineOfItsElevation)
{
    Components::SkyEnvironment sky{};
    sky.TimeOfDayHours = 10.0f;
    sky.Latitude = 40.0f;
    const Rendering::SolarPosition sun = Rendering::SolarPositionAtHour(
        Rendering::MakeSolarFrame(Components::SkySunPath::PathAngles(sky)), sky.TimeOfDayHours);
    ASSERT_GT(sun.ElevationDegrees, 30.0f);
    ASSERT_LT(sun.ElevationDegrees, 50.0f);

    FinalizedLights lights;
    lights.Add(kOverheadSunLux, sun.ElevationDegrees);
    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    const float expected = std::log2(kOverheadSunLux * DegreesSine(sun.ElevationDegrees) / 2.5f);
    EXPECT_NEAR(exposure.GetEv100(), expected, 1e-3f);
}

// The sky adds to the direct light; with no direct light the sky alone sets the exposure.
TEST(SceneLightExposure, TheSkyAddsToTheSunAndStandsAloneWithoutIt)
{
    constexpr float kSkyLux = 15800.0f;
    const SceneLightIncidence withSun{.DirectLux = kOverheadSunLux * DegreesSine(20.0f), .SkyLux = kSkyLux};
    EXPECT_NEAR(SceneLightEv100(withSun), std::log2((kOverheadSunLux * DegreesSine(20.0f) + kSkyLux) / 2.5f),
                1e-4f);

    const SceneLightIncidence skyOnly{.SkyLux = 8600.0f};
    EXPECT_NEAR(SceneLightEv100(skyOnly), std::log2(8600.0f / 2.5f), 1e-4f);
}

// A scene with no sun and no sky has no daylight to derive from and falls back to the Scene
// View's former fixed default.
TEST(SceneLightExposure, NoLightFallsBackToTheStatedEv)
{
    EXPECT_EQ(Editor::SceneLightFallbackEv100(), Components::kDefaultManualExposureEv);
    EXPECT_EQ(SceneLightEv100(SceneLightIncidence{}), Components::kDefaultManualExposureEv);
}

// The controller reads the sun's direction and re-derives when the sun changes.
TEST(SceneLightExposure, TheControllerFollowsTheWorldsSun)
{
    FinalizedLights lights;
    const Editor::SceneLightEnvironment noEnvironment{}; // the sun alone
    const size_t sun = lights.Add(kOverheadSunLux, 90.0f);

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), noEnvironment), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2(kOverheadSunLux / 2.5f), 1e-3f);

    FinalizedLights::Aim(lights.At(sun), 30.0f);
    ASSERT_EQ(exposure.Update(lights.Finalize(), noEnvironment), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2(kOverheadSunLux * 0.5f / 2.5f), 1e-3f)
        << "a sun 30 degrees up delivers half its lux to a horizontal surface";
}

// Every directional light lights the ground: two suns and the sky add on a horizontal surface.
TEST(SceneLightExposure, TwoDirectionalLightsAddTheirLightOnTheGround)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    lights.Add(50000.0f, 30.0f);
    Editor::SceneLightEnvironment environment{};
    environment.Lit = true;
    environment.Lux = 10000.0f;

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), environment), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2((kOverheadSunLux + 50000.0f * 0.5f + 10000.0f) / 2.5f), 0.01f);
}

// A directional light below the horizon lights no horizontal surface, however bright it is.
TEST(SceneLightExposure, ALightBelowTheHorizonAddsNothing)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    lights.Add(120000.0f, -5.0f);

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2(kOverheadSunLux / 2.5f), 0.01f);
}

// A light with Casts Light off adds nothing. (A disabled Light or an inactive entity is never
// extracted, so it never reaches the list.)
TEST(SceneLightExposure, ALightThatCastsNoLightAddsNothing)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    lights.At(lights.Add(200000.0f, 90.0f)).castsLight = 0u;

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2(kOverheadSunLux / 2.5f), 0.01f);
}

// Adding a second directional light re-derives the exposure with its light included.
TEST(SceneLightExposure, AddingALightRederives)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Unchanged);

    lights.Add(50000.0f, 30.0f);
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2((kOverheadSunLux + 50000.0f * 0.5f) / 2.5f), 0.01f);
}

// The renderer shades the kMaxDirectionalLights strongest directionals and drops the rest; the meter
// reads what is drawn, so a fifth, weakest sun adds nothing.
TEST(SceneLightExposure, ADirectionalPastTheRenderersCapAddsNothing)
{
    ASSERT_EQ(Engine::Renderer::kMaxDirectionalLights, 4u);
    FinalizedLights lights;
    lights.Add(40000.0f, 90.0f);
    lights.Add(30000.0f, 90.0f);
    lights.Add(20000.0f, 90.0f);
    lights.Add(10000.0f, 90.0f);
    lights.Add(5000.0f, 90.0f);

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2(100000.0f / 2.5f), 0.01f);
}

// An environment that lights the scene before its first irradiance reading lands holds the
// derivation: no EV before any reading, and the previous EV while one is pending, never a
// sun-only value.
TEST(SceneLightExposure, AnEnvironmentWithoutAReadingHolds)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    Editor::SceneLightEnvironment pending{};
    pending.Lit = true;

    Editor::SceneLightExposure fresh;
    EXPECT_EQ(fresh.Update(lights.Finalize(), pending), Editor::SceneLightUpdate::NotYet);

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), Editor::SceneLightEnvironment{}), Editor::SceneLightUpdate::Rederived);
    const float sunOnly = exposure.GetEv100();
    EXPECT_EQ(exposure.Update(lights.Finalize(), pending), Editor::SceneLightUpdate::Held);
    EXPECT_EQ(exposure.GetEv100(), sunOnly);
}

// A reading of L lux from an environment at IBL intensity k adds k x L to the sun's term: the
// light the scene is lit with, not the cube's raw value.
TEST(SceneLightExposure, TheEnvironmentAddsItsReadingTimesItsIntensity)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    Editor::SceneLightEnvironment environment{};
    environment.Lit = true;
    environment.Lux = 20000.0f;
    environment.IblIntensity = 0.5f;

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), environment), Editor::SceneLightUpdate::Rederived);
    EXPECT_NEAR(exposure.GetEv100(), std::log2((kOverheadSunLux + 0.5f * 20000.0f) / 2.5f), 1e-3f);
}

// The derivation is worked out again for a new reading and not for the same one.
TEST(SceneLightExposure, ANewReadingRederivesAndTheSameOneDoesNot)
{
    FinalizedLights lights;
    lights.Add(kOverheadSunLux, 90.0f);
    Editor::SceneLightEnvironment environment{};
    environment.Lit = true;
    environment.Lux = 16000.0f;

    Editor::SceneLightExposure exposure;
    ASSERT_EQ(exposure.Update(lights.Finalize(), environment), Editor::SceneLightUpdate::Rederived);
    const float first = exposure.GetEv100();
    EXPECT_EQ(exposure.Update(lights.Finalize(), environment), Editor::SceneLightUpdate::Unchanged);

    environment.Lux = 4000.0f;
    EXPECT_EQ(exposure.Update(lights.Finalize(), environment), Editor::SceneLightUpdate::Rederived);
    EXPECT_LT(exposure.GetEv100(), first) << "less sky light, a lower EV";
}

// The mode is a user preference: it is saved and read back.
TEST(SceneViewExposureMode, TheModePersistsInThePreferences)
{
    const std::filesystem::path root = TestUtils::MakeUniqueTempDirectory("SceneViewExposureMode");
    Testing::SetEnvVar("GE_EDITOR_USER_DATA_ROOT", root.string().c_str());
    SceneViewSettings& settings = SceneViewSettings::Get();
    const SceneViewExposureMode original = settings.GetExposureMode();

    settings.SetExposureMode(SceneViewExposureMode::Fixed, /*fromUserSwitch=*/false);
    {
        auto prefs = Editor::OpenEditorPreferences();
        std::string err;
        ASSERT_TRUE(prefs.Load(&err)) << err;
        double stored = -1.0;
        ASSERT_TRUE(prefs.TryGetDouble("sceneView.exposureMode", stored));
        EXPECT_EQ(static_cast<int>(stored), static_cast<int>(SceneViewExposureMode::Fixed));
        prefs.SetDouble("sceneView.exposureMode", static_cast<double>(SceneViewExposureMode::SceneLight));
        ASSERT_TRUE(prefs.Save(&err)) << err;
    }
    settings.Load();
    EXPECT_EQ(settings.GetExposureMode(), SceneViewExposureMode::SceneLight);

    settings.SetExposureMode(original, /*fromUserSwitch=*/false);
    Testing::SetEnvVar("GE_EDITOR_USER_DATA_ROOT", "");
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

// Switching from Scene Light to Fixed starts Fixed at the derived EV, on a slider detent, so the
// image does not jump at the switch.
TEST(SceneViewExposureMode, SwitchingToFixedStartsAtTheShownEv)
{
    const std::filesystem::path root = TestUtils::MakeUniqueTempDirectory("SceneViewExposureHandoff");
    Testing::SetEnvVar("GE_EDITOR_USER_DATA_ROOT", root.string().c_str());
    SceneViewSettings& settings = SceneViewSettings::Get();
    const SceneViewExposureMode originalMode = settings.GetExposureMode();
    const float originalFixed = settings.GetPostFxFixedExposureEv();

    settings.SetExposureMode(SceneViewExposureMode::SceneLight, /*fromUserSwitch=*/false);
    settings.SetPostFxFixedExposureEv(9.0f);
    settings.SetLastShownExposureEv(13.37f);
    settings.SetExposureMode(SceneViewExposureMode::Fixed, /*fromUserSwitch=*/true);
    EXPECT_FLOAT_EQ(settings.GetPostFxFixedExposureEv(), 13.4f);

    settings.SetPostFxFixedExposureEv(originalFixed);
    settings.SetExposureMode(originalMode, /*fromUserSwitch=*/false);
    Testing::SetEnvVar("GE_EDITOR_USER_DATA_ROOT", "");
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}
