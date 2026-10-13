#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace GameEngine
{
namespace Engine::Renderer
{
class RenderServices;
struct ExtractedLight;
}

namespace Editor
{

// The daylight falling on a horizontal surface, the incident reading the Scene View's Scene Light
// exposure is derived from.
struct SceneLightIncidence
{
    // What the directional lights the renderer shades deliver to a horizontal surface, in lux: the
    // sum, over Engine::Renderer::SelectShadedDirectionals, of each light's delivered lux times
    // max(0, the sine of its elevation), so a light below the horizon adds nothing.
    float DirectLux = 0.0f;
    // The image-based light on an up-facing surface, in lux, as the scene is lit by it: the sky's,
    // or a reflection probe's while one replaces the sky.
    float SkyLux = 0.0f;
};

// The EV100 a scene with no daylight at all falls back to: the Scene View's former fixed default,
// tuned for judging colour under a clear noon sky (Components::kDefaultManualExposureEv).
float SceneLightFallbackEv100();

// The incident-meter exposure for `incidence`: EV100 = log2(E / 2.5), with
// E = DirectLux + SkyLux, the light on a horizontal surface. Clamped to the
// Scene View's EV range; SceneLightFallbackEv100() when E is not positive.
float SceneLightEv100(const SceneLightIncidence& incidence);

// The image-based half of the reading, as the renderer holds it now.
struct SceneLightEnvironment
{
    // An environment source with content lights the scene: the sky, or a reflection probe that
    // replaces it.
    bool Lit = false;
    // Its newest irradiance reading on an up-facing surface, in lux; absent until the first lands.
    std::optional<float> Lux;
    // The source's IblIntensity, which ibl.glsl multiplies into what it samples.
    float IblIntensity = 1.0f;
};

// Reads the environment term from the IBL feature's active source and the irradiance readback,
// creating the readback on first use.
SceneLightEnvironment ReadSceneLightEnvironment(Engine::Renderer::RenderServices& services);

// What one SceneLightExposure::Update did.
enum class SceneLightUpdate : uint8_t
{
    NotYet,    // no EV yet: the environment's first reading has not landed
    Held,      // the environment lights the scene but has no reading yet; the previous EV stands
    Unchanged, // the inputs are those of the current EV
    Rederived, // an input changed, and the EV was worked out again
};

// Derives the Scene View's Scene Light exposure from the directional lights the renderer shades and
// the sky. Updated every frame, it re-derives only when an input changed: the shaded directional
// lights' sum on a horizontal surface (a light added, removed, enabled, disabled, turned or made
// brighter), or a new
// reading of the image-based light, which lands only after the sky or an active reflection probe
// bakes again. It never meters the image.
class SceneLightExposure
{
  public:
    // Reads the directional lights the renderer shades from the world's finalized light list
    // (RenderServices::GetWorldLights), takes the environment term, and re-derives when either
    // changed. GetEv100 is valid for every result but NotYet.
    SceneLightUpdate Update(std::span<const Engine::Renderer::ExtractedLight> sortedLights,
                            const SceneLightEnvironment& environment);
    float GetEv100() const { return m_Ev100; }

  private:
    SceneLightIncidence m_Inputs{};
    float m_Ev100 = 0.0f;
    bool m_HasEv = false;
};

} // namespace Editor
} // namespace GameEngine
