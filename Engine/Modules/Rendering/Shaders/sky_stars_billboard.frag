#version 450

#include "Includes/star_sparkle.glsl"

layout(location = 0) in vec2 vQuadPos;
layout(location = 1) flat in float vBrightness;
layout(location = 2) flat in float vTwinklePhase;
layout(location = 3) flat in float vRotAngle;
layout(location = 4) flat in float vSizeScale;
layout(location = 5) flat in float vColorTemp;
layout(location = 6) flat in vec2 vTwinkleRange;

layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 1) uniform SkyUBO
{
    vec3  cameraPositionWS;   float exposureEV;
    vec3  cameraRightWS;      float pad0;
    vec3  cameraUpWS;         float pad1;
    vec3  cameraForwardWS;    float pad2;
    vec3  sunDirectionWS;     float sunAngularRadius;
    vec3  moonDirectionWS;    float moonIntensity;
    float moonAngularRadius;
    float skyRotateAroundZenithRadians;
    float moonUboPad1;
    float moonUboPad2;
    vec3  sunColor;           float sunIntensity;
    float timeOfDayHours;     float skyPad0;
    float viewportAspect;     float tanHalfFovY;
    vec2  viewportResolution; float nightSkyBlend; float skyTimeSeconds;
    vec4  starSizeShape;
    vec4  starTwinkleParams;
    vec4  starTwinkleAmp;
    vec4  starDensityHorizon;
    vec4  starSizeRange;
} uSky;

void main()
{
    // Taken before the discard, in uniform control flow, for the filtered sparkle below.
    vec2 quadDx = dFdx(vQuadPos);
    vec2 quadDy = dFdy(vQuadPos);
    float r = length(vQuadPos);
    if (r > 1.0)
        discard;

    float starRadiusFrac = clamp(uSky.starSizeShape.y, 0.001, 2.0) * vSizeScale;
    float pointRadiusFrac = clamp(uSky.starSizeShape.z, 0.001, 2.0) * vSizeScale;
    float maxExtent = max(starRadiusFrac, pointRadiusFrac);
    float pointFrac = pointRadiusFrac / maxExtent;

    float pointAmount = clamp(uSky.starSizeShape.w, 0.0, 1.0);
    float glowFalloff = clamp(uSky.starTwinkleParams.w, 0.5, 16.0);
    float coreSize = clamp(uSky.starSizeRange.z, 0.0, 0.6);

    float cs = cos(vRotAngle);
    float sn = sin(vRotAngle);
    vec2 rotPos = vec2(cs * vQuadPos.x - sn * vQuadPos.y,
                       sn * vQuadPos.x + cs * vQuadPos.y);
    mat2 toStar = mat2(cs, sn, -sn, cs) * (1.0 / pointFrac);
    float shape = GE_StarSparkleFiltered(rotPos / pointFrac, toStar * quadDx, toStar * quadDy,
                                         starRadiusFrac / pointRadiusFrac, pointAmount, coreSize,
                                         glowFalloff);

    float starBrightnessSetting = clamp(uSky.starDensityHorizon.w, 0.0, 5.0);

    vec3 warmTint = vec3(1.0, 0.85, 0.7);
    vec3 coolTint = vec3(0.7, 0.85, 1.0);
    vec3 starColor = mix(warmTint, coolTint, vColorTemp);

    // The star's full light is a diamond star's (starTwinkleAmp.w and .z scale the sparkle and the
    // glow to it: ComputeStarSparkleEnergy) times the mean of the twinkle range (Twinkle Intensity
    // to starTwinkleAmp.y, clamped as the range always was). Of that, the sparkle carries a steady
    // (1 - kGlowShare) and the glow the rest, scaled by the twinkle's phase over its mean, so the
    // glow brightens with the twinkle and the sparkle's shape stays put. The star's own range then
    // scales the whole star (below): 0.25 of the full light on average over the catalog and a
    // twinkle period.
    const float kTwinklePhaseMean = 0.4244; // mean of ((1 + sin)/2)^1.5 over a period
    const float kGlowShare = 0.5;
    float twinkleFloor = clamp(uSky.starTwinkleAmp.x, 0.0, 2.0);
    float twinklePeak = clamp(uSky.starTwinkleAmp.y, twinkleFloor, 4.0);
    float meanTwinkle = mix(twinkleFloor, twinklePeak, kTwinklePhaseMean);
    float glow = GE_StarGlowFiltered(rotPos / pointFrac, toStar * quadDx, toStar * quadDy);
    float sparkleLight = (1.0 - kGlowShare) * uSky.starTwinkleAmp.w * shape;
    float glowLight = kGlowShare * (vTwinklePhase / kTwinklePhaseMean) * uSky.starTwinkleAmp.z * glow;
    // The twinkle also dims the whole star at its low phase, by the star's own range
    // (GE_StarTwinkleRange): a brightness change on the additive star, which on a dark sky reads
    // exactly as a fade.
    float twinkleBrightness = GE_StarTwinkleBrightness(vTwinkleRange, vTwinklePhase);
    float intensity = vBrightness * starBrightnessSetting * meanTwinkle * twinkleBrightness *
                      (sparkleLight + glowLight);

    oColor = vec4(starColor * intensity, 0.0);
}
