// The star's energy gains (Rendering/Sky/StarSparkleEnergy.h): the factors that give the
// four-pointed sparkle and the twinkle glow the integrated light of a diamond star with the same
// settings. Pure CPU maths, no device.

#include "Rendering/Sky/StarSparkleEnergy.h"

#include <gtest/gtest.h>

#include <cmath>

using GameEngine::Rendering::ComputeStarSparkleEnergy;

namespace
{

float Gain(float pointAmount, float coreSize, float falloff)
{
    return ComputeStarSparkleEnergy(pointAmount, coreSize, falloff).SparkleGain;
}

} // namespace

// With no points both shapes are the same round core, so there is nothing to make up.
TEST(StarSparkleEnergy, NoPointsNeedNoGain)
{
    EXPECT_NEAR(Gain(0.0f, 0.15f, 10.0f), 1.0f, 1e-6f);
}

// The default star (Sparkle Points 0.5, Core Size 0.15, Glow Falloff 10) loses about half its
// light to the sparkle's narrower points, so the gain is about 2 (2.017 from an independent
// 512 x 2048 integration), and it grows with the points' share of the star.
TEST(StarSparkleEnergy, DefaultStarGainsAboutTwoAndGrowsWithThePoints)
{
    const float defaults = Gain(0.5f, 0.15f, 10.0f);
    EXPECT_NEAR(defaults, 2.017f, 2.017f * 1e-2f);
    EXPECT_LT(Gain(0.1f, 0.15f, 10.0f), defaults);
    EXPECT_GT(Gain(1.0f, 0.15f, 10.0f), defaults);
}

// Out-of-range settings are clamped as the star shader clamps them, never integrated raw.
TEST(StarSparkleEnergy, ArgumentsClampLikeTheShader)
{
    EXPECT_FLOAT_EQ(Gain(2.0f, 0.15f, 10.0f), Gain(1.0f, 0.15f, 10.0f));
    EXPECT_FLOAT_EQ(Gain(0.5f, 0.9f, 10.0f), Gain(0.5f, 0.6f, 10.0f));
    EXPECT_FLOAT_EQ(Gain(0.5f, 0.15f, 40.0f), Gain(0.5f, 0.15f, 16.0f));
}

// The glow carries a diamond star's light: the diamond's integral over the glow's closed form,
// (4/3) / 20 at k = 3. At the default shape that is 0.9666 (an independent 256 x 1024
// integration of the diamond, 0.06444, over 0.06667), and it follows the diamond, not the
// sparkle.
TEST(StarSparkleEnergy, GlowCarriesTheDiamondsLight)
{
    const auto defaults = ComputeStarSparkleEnergy(0.5f, 0.15f, 10.0f);
    EXPECT_NEAR(defaults.GlowGain, 0.9666f, 0.9666f * 1e-2f);
    EXPECT_LT(ComputeStarSparkleEnergy(0.1f, 0.15f, 10.0f).GlowGain, defaults.GlowGain)
        << "fewer points make a dimmer diamond, so less light for the glow to carry";
}
