#ifndef GE_SKY_ATMOSPHERE_UBO_GLSL
#define GE_SKY_ATMOSPHERE_UBO_GLSL

// Canonical std140 AtmosphereUBO, declared once and shared by every sky pass.
// Byte-identical to AtmosphereParametersGPU (240 B) in
// Rendering/Sky/SkyRenderer.h, whose field offsets are all static_asserted --
// keep this block in lock-step with that struct. Every sky pass uploads the
// full 240 B AtmosphereParametersGPU (the capture packs it at
// kCaptureAtmoUboOffset), so a pass may declare the whole block here even when
// it only reads a prefix of it.
//
// The bind point differs per pass, so define GE_ATMOSPHERE_UBO_BINDING before
// including. It defaults to binding 2 (sky-view / render / multiscatter /
// capture); the transmittance pass overrides it to 1.
#ifndef GE_ATMOSPHERE_UBO_BINDING
#define GE_ATMOSPHERE_UBO_BINDING 2
#endif

layout(set = 0, binding = GE_ATMOSPHERE_UBO_BINDING) uniform AtmosphereUBO
{
    float planetRadius;
    float atmosphereRadius;
    float rayleighScaleHeight;
    float mieScaleHeight;
    vec3  betaRayleigh;   float pad0;
    vec3  betaMie;        float mieG;
    vec3  groundAlbedo;
    float groundBrightness;
    vec3  groundNightColor;
    vec3  groundHorizonColor;
    vec3  groundHorizonNightColor;
    vec3  nightSkyHorizonColor;
    float nightSkyHorizonPad;
    float groundHorizonDayCosWidth;
    float groundHorizonNightCosWidth;
    float belowHorizonBlendSharpness;
    float belowHorizonDarkness;
    vec2  belowHorizonDarkPadding;
    float belowHorizonDarkColorStd140PrePad0;
    float belowHorizonDarkColorStd140PrePad1;
    float belowHorizonDarkColorStd140PrePad2;
    vec3  belowHorizonDarkColor;
    uint  belowHorizonMode;
    float groundHazeStrength;
    vec3  mieAbsorption;  float ozoneCenterHeight;
    vec3  ozoneAbsorption; float ozoneHalfWidth;
} uAtmos;

// The medium at height h above the ground (metres), shared by every pass that integrates the
// atmosphere: Rayleigh and Mie scattering on their exponential profiles, Mie absorption on the
// Mie profile, ozone absorption on its tent (Hillaire 2020, section 3 and table 1).
struct GE_SkyMedium
{
    vec3 scatterRayleigh;
    vec3 scatterMie;
    vec3 extinction;
};

GE_SkyMedium GE_SkyMediumAt(float h)
{
    float rayleighDensity = exp(-h / uAtmos.rayleighScaleHeight);
    float mieDensity = exp(-h / uAtmos.mieScaleHeight);
    float ozoneDensity = max(0.0, 1.0 - abs(h - uAtmos.ozoneCenterHeight) / uAtmos.ozoneHalfWidth);
    GE_SkyMedium m;
    m.scatterRayleigh = uAtmos.betaRayleigh * rayleighDensity;
    m.scatterMie = uAtmos.betaMie * mieDensity;
    m.extinction = m.scatterRayleigh + (uAtmos.betaMie + uAtmos.mieAbsorption) * mieDensity +
                   uAtmos.ozoneAbsorption * ozoneDensity;
    return m;
}

#endif
