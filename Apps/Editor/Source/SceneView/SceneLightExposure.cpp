#include "SceneView/SceneLightExposure.h"

#include "Components/Rendering/Light.h"
#include "Components/Rendering/LightPhotometry.h"
#include "Components/Rendering/PostProcessVolume.h"
#include "Editor/Settings/SceneViewSettings.h"
#include "Engine/Rendering/RenderServices.h"
#include "Engine/Rendering/IEnvironmentSource.h"
#include "Engine/Rendering/ImageBasedLightingFeature.h"
#include "Engine/Rendering/IrradianceReadbackFeature.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace GameEngine::Editor
{

namespace
{
// The incident meter's calibration for a flat receiver: EV100 = log2(E x S / C) with S = 100 and
// C = 250 (ISO 2720), so E / 2.5 with E in lux. A 100 000 lx overhead sun reads 15.3.
constexpr float kIncidentMeterLuxPerEv0 = 2.5f;

// What one shaded light delivers to a horizontal surface, in lux: its delivered lux (colour
// luminance times intensity, as SkySunIlluminance::DeliveredLux resolves it at extraction) times the
// up component of the direction toward it (directionWS is the shine direction, so minus it). 0 for a
// light below the horizon.
float HorizontalLux(const Engine::Renderer::ExtractedLight& light)
{
    const float elevationSine = -light.directionWS[1];
    const float deliveredLux =
        Components::UnitlessToLightIntensity(Engine::Renderer::DeliveredLuminance(light), Components::LightUnit::Lux);
    return deliveredLux * std::max(0.0f, elevationSine);
}

// The sum of HorizontalLux over the directionals the renderer shades (SelectShadedDirectionals): the
// primary and the strongest emitting secondaries, up to kMaxDirectionalLights; the rest are not drawn.
float DirectionalLux(std::span<const Engine::Renderer::ExtractedLight> sortedLights)
{
    std::array<const Engine::Renderer::ExtractedLight*, Engine::Renderer::kMaxDirectionalLights> shaded{};
    const uint32_t count = Engine::Renderer::SelectShadedDirectionals(sortedLights, shaded);
    float lux = 0.0f;
    for (uint32_t i = 0; i < count; ++i)
        if (shaded[i]->castsLight != 0u)
            lux += HorizontalLux(*shaded[i]);
    return lux;
}

bool SameIncidence(const SceneLightIncidence& a, const SceneLightIncidence& b)
{
    return a.DirectLux == b.DirectLux && a.SkyLux == b.SkyLux;
}
} // namespace

float SceneLightFallbackEv100()
{
    return Components::kDefaultManualExposureEv;
}

float SceneLightEv100(const SceneLightIncidence& incidence)
{
    const float horizontalLux = std::max(0.0f, incidence.DirectLux) + std::max(0.0f, incidence.SkyLux);
    if (!(horizontalLux > 0.0f) || !std::isfinite(horizontalLux))
        return SceneLightFallbackEv100();
    return std::clamp(std::log2(horizontalLux / kIncidentMeterLuxPerEv0), SceneViewSettings::kMinExposureEv,
                      SceneViewSettings::kMaxExposureEv);
}

SceneLightEnvironment ReadSceneLightEnvironment(Engine::Renderer::RenderServices& services)
{
    SceneLightEnvironment out;
    const auto* ibl = services.GetFeature<Engine::Renderer::ImageBasedLightingFeature>();
    const Engine::Renderer::IEnvironmentSource* source = ibl ? ibl->GetEnvironmentSource() : nullptr;
    if (!source || !source->HasActiveContent())
        return out;
    out.Lit = true;
    out.IblIntensity = source->IblIntensity();
    float lux = 0.0f;
    if (services.EnsureFeature<Engine::Renderer::IrradianceReadbackFeature>().TryGetUpIrradianceLux(lux))
        out.Lux = lux;
    return out;
}

SceneLightUpdate SceneLightExposure::Update(std::span<const Engine::Renderer::ExtractedLight> sortedLights,
                                            const SceneLightEnvironment& environment)
{
    SceneLightIncidence inputs{};
    inputs.DirectLux = DirectionalLux(sortedLights);

    // The image-based term counts only while an environment lights the scene. Its reading lands a
    // frame or two after each bake; until the first one lands the derivation holds, so it never
    // flashes a value without the sky.
    if (environment.Lit)
    {
        if (!environment.Lux)
            return m_HasEv ? SceneLightUpdate::Held : SceneLightUpdate::NotYet;
        // Lit as ibl.glsl lights it: the baked irradiance times the source's IBL intensity.
        inputs.SkyLux = *environment.Lux * std::max(0.0f, environment.IblIntensity);
    }

    if (m_HasEv && SameIncidence(inputs, m_Inputs))
        return SceneLightUpdate::Unchanged;
    m_Inputs = inputs;
    m_Ev100 = SceneLightEv100(inputs);
    m_HasEv = true;
    return SceneLightUpdate::Rederived;
}

} // namespace GameEngine::Editor
