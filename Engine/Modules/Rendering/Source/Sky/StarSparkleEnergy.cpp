#include "Rendering/Sky/StarSparkleEnergy.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace GameEngine::Rendering
{
namespace
{

// Midpoint-rule resolution over one octant (0 to 45 degrees; both shapes are symmetric under
// x <-> y and either sign) and the unit radius. At 24 x 96 both gains are within 1 percent of
// a 256 x 1024 reference across the settings' ranges, and one computation takes about 0.06 ms in
// Release: it runs on the render thread on every frame a star setting is dragged.
constexpr int kAngleSteps = 24;
constexpr int kRadiusSteps = 96;

// star_sparkle.glsl's GE_StarFalloff.
float Falloff(float d, float coreSize, float falloff)
{
    const float n = std::clamp(d, 0.0f, 1.0f);
    const float t = (n <= coreSize) ? 0.0f : (n - coreSize) / (1.0f - coreSize);
    return std::pow(1.0f - t, falloff);
}

// Integral over one octant of max(core(r), pointAmount * points(r * reachScale(theta))), where
// the points' metric at angle theta is r times reachScale(theta).
template <typename MetricScale>
double OctantEnergy(MetricScale reachScale, float pointAmount, float coreSize, float falloff)
{
    constexpr double kOctant = std::numbers::pi / 4.0;
    const double dTheta = kOctant / kAngleSteps;
    const double dRadius = 1.0 / kRadiusSteps;
    double sum = 0.0;
    for (int a = 0; a < kAngleSteps; ++a)
    {
        const double theta = (a + 0.5) * dTheta;
        const float scale = reachScale(static_cast<float>(std::cos(theta)), static_cast<float>(std::sin(theta)));
        for (int i = 0; i < kRadiusSteps; ++i)
        {
            const float r = static_cast<float>((i + 0.5) * dRadius);
            const float core = Falloff(r / kStarSparkleCoreRadius, coreSize, falloff);
            const float points = pointAmount * Falloff(r * scale, coreSize, falloff);
            sum += static_cast<double>(std::max(core, points)) * r;
        }
    }
    return sum * dRadius * dTheta;
}

float DiamondScale(float c, float s)
{
    return c + s;
}

// star_sparkle.glsl's GE_StarPointMetric on the unit vector (c, s).
float SparkleScale(float c, float s)
{
    const float m = std::sqrt(c) + std::sqrt(s);
    return m * m;
}

} // namespace

StarSparkleEnergy ComputeStarSparkleEnergy(float pointAmount, float coreSize, float falloff)
{
    const float amount = std::clamp(pointAmount, 0.0f, 1.0f);
    const float core = std::clamp(coreSize, 0.0f, 0.6f);
    const float fall = std::clamp(falloff, 0.5f, 16.0f);
    // Eight octants make the disc.
    const double diamond = 8.0 * OctantEnergy(DiamondScale, amount, core, fall);
    const double sparkle = 8.0 * OctantEnergy(SparkleScale, amount, core, fall);
    const double k = kStarGlowFalloff;
    const double glow = (4.0 / 3.0) / ((k + 1.0) * (k + 2.0));
    StarSparkleEnergy energy;
    energy.SparkleGain = static_cast<float>(diamond / sparkle);
    energy.GlowGain = static_cast<float>(diamond / glow);
    return energy;
}

} // namespace GameEngine::Rendering
