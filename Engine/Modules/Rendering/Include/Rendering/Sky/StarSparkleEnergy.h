#pragma once

namespace GameEngine::Rendering
{

/// Radius of a night-sky star's round core as a fraction of its points' reach: the billboard's
/// star radius over its diamond radius (SkyRenderNode's kBaseStarRadius / kBaseDiamondRadius),
/// which Star Size scales together.
inline constexpr float kStarSparkleCoreRadius = 0.4f;

/// Exponent of a star's twinkle glow, (1 - m)^k on the sparkle's four-pointed metric m
/// (star_sparkle.glsl, GE_StarGlow): integral (4/3) / ((k + 1) (k + 2)), the region m <= 1 having
/// area 2/3.
inline constexpr float kStarGlowFalloff = 3.0f;

/// Gains that give a night-sky star's two light terms the integrated light of a diamond star
/// with the same Sparkle Points (StarDiamondShape), Core Size and Glow Falloff (the same round
/// core under four points on the convex |x| + |y| metric), so the sparkle's shape does not change
/// a star's light. Each star's twinkle range then scales that light (star_sparkle.glsl,
/// GE_StarTwinkleRange): 0.25 of it on average over the catalog and a twinkle period.
struct StarSparkleEnergy
{
    /// Multiplies the four-pointed sparkle (GE_StarSparkle), whose concave points cover less of
    /// the sprite than the diamond's: about 2 at the default shape.
    float SparkleGain = 1.0f;
    /// Multiplies the twinkle glow (GE_StarGlow), the star-shaped glow the twinkle brightens.
    float GlowGain = 1.0f;
};

/// Both gains, from integrals over the unit disc where every term ends: the sparkle's and the
/// diamond's numerically, the glow's in closed form. Arguments are clamped as the star shader
/// clamps them. With no points (pointAmount 0) the sparkle is the diamond and SparkleGain is 1.
StarSparkleEnergy ComputeStarSparkleEnergy(float pointAmount, float coreSize, float falloff);

} // namespace GameEngine::Rendering
