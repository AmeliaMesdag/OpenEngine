// The night sky's star catalog (Rendering/Sky/StarCatalog.h). Pure CPU, no device.

#include "Source/Sky/StarCatalog.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using GameEngine::Rendering::BuildStarCatalog;
using GameEngine::Rendering::kStarRotationSpreadRadians;

// The sparkles turn up to 0.6 rad off the screen's axes, either way, and the catalog uses that
// whole spread: a narrower spread reads as one stamp repeated across the sky.
TEST(StarCatalog, RotationsSpreadAcrossPlusMinusSixTenthsOfARadian)
{
    EXPECT_FLOAT_EQ(kStarRotationSpreadRadians, 0.6f);

    const auto stars = BuildStarCatalog(4096);
    ASSERT_EQ(stars.size(), 4096u);
    float lowest = 0.0f;
    float highest = 0.0f;
    for (const auto& star : stars)
    {
        const float rotation = star.properties[1];
        EXPECT_LE(std::abs(rotation), kStarRotationSpreadRadians);
        lowest = std::min(lowest, rotation);
        highest = std::max(highest, rotation);
    }
    EXPECT_LT(lowest, -0.58f) << "the catalog does not reach the spread's negative end";
    EXPECT_GT(highest, 0.58f) << "the catalog does not reach the spread's positive end";
}

// The catalog is seeded: the same sky on every run. The first and last of 64 stars are pinned at
// seed 42 as MSVC's standard library draws them; a change of seed or of draw order moves them.
// Another standard library may draw different floats from the same seed (the distribution is
// not specified bit for bit), so the values are this build's, not every machine's.
TEST(StarCatalog, IsTheSameSkyEveryRun)
{
    const auto stars = BuildStarCatalog(64);
    ASSERT_EQ(stars.size(), 64u);
    const float first[8] = {-0.567671895f, 0.593085885f, 0.570962131f, 0.965499997f,
                            0.820121765f,  0.278392702f, 0.779690981f, 0.598658442f};
    const float last[8] = {-0.353423506f, 0.136617228f,  0.925433695f, 0.679778099f,
                           0.712409675f,  0.0105768442f, 0.367715776f, 0.636332572f};
    for (int k = 0; k < 4; ++k)
    {
        EXPECT_NEAR(stars.front().dirAndBrightness[k], first[k], 1e-6f) << "star 0, dirAndBrightness[" << k << "]";
        EXPECT_NEAR(stars.front().properties[k], first[4 + k], 1e-6f) << "star 0, properties[" << k << "]";
        EXPECT_NEAR(stars.back().dirAndBrightness[k], last[k], 1e-6f) << "star 63, dirAndBrightness[" << k << "]";
        EXPECT_NEAR(stars.back().properties[k], last[4 + k], 1e-6f) << "star 63, properties[" << k << "]";
    }
}
