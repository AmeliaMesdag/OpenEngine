#include "StarCatalog.h"

#include <cmath>
#include <numbers>
#include <random>

namespace GameEngine::Rendering
{

std::vector<StarInstanceGPU> BuildStarCatalog(uint32_t starCount)
{
    std::vector<StarInstanceGPU> stars;
    stars.reserve(starCount);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

    for (uint32_t i = 0; i < starCount; ++i)
    {
        const float u = dist01(rng);
        const float v = dist01(rng);
        const float theta = kTwoPi * u;
        const float phi = std::acos(2.0f * v - 1.0f);
        const float sinPhi = std::sin(phi);

        StarInstanceGPU star{};
        star.dirAndBrightness[0] = sinPhi * std::cos(theta);
        star.dirAndBrightness[1] = std::cos(phi);
        star.dirAndBrightness[2] = sinPhi * std::sin(theta);
        star.dirAndBrightness[3] = 0.3f + 0.7f * dist01(rng);
        star.properties[0] = 0.6f + 1.2f * dist01(rng);
        star.properties[1] = kStarRotationSpreadRadians * (2.0f * dist01(rng) - 1.0f);
        star.properties[2] = dist01(rng);
        star.properties[3] = dist01(rng);
        stars.push_back(star);
    }
    return stars;
}

} // namespace GameEngine::Rendering
