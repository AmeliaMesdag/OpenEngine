#pragma once

#include "Mathematics/Matrix4x4.h"

#include <cmath>

namespace GameEngine
{
namespace Engine::Renderer
{

inline bool IsOrthographicProjectionLH_ZO(const Mathematics::Matrix4x4& proj)
{
    return std::abs(proj[3][3] - 1.0f) < 1e-4f && std::abs(proj[2][3]) < 1e-4f;
}

// Extract near and far planes from a finite LH reverse-Z [1,0] projection, perspective
// or orthographic, addressed as proj[column][row]. Perspective: near = proj[3][2] /
// (1 - proj[2][2]), far = -proj[3][2] / proj[2][2]. Degenerate input gives defaults.
inline void ExtractNearFarLH_ZO(const Mathematics::Matrix4x4& proj, float& outNear, float& outFar)
{
    const float m22 = proj[2][2];
    const float m32 = proj[3][2];

    float zn = 0.1f;
    float zf = 1000.0f;

    if (IsOrthographicProjectionLH_ZO(proj))
    {
        // Reverse-Z orthographic:
        //   m22 = -1/(far-near), m32 = far/(far-near)
        const float range = -1.0f / m22;
        zf = m32 * range;
        zn = zf - range;
    }
    else if (std::abs(m22) > 1e-6f && std::abs(1.0f - m22) > 1e-6f)
    {
        // Reverse-Z perspective:
        //   m22 = -near/(far-near), m32 = near*far/(far-near)
        zn = m32 / (1.0f - m22);
        zf = -m32 / m22;
    }

    if (!(zn > 0.0f) || !std::isfinite(zn))
        zn = 0.1f;
    if (!(zf > zn) || !std::isfinite(zf))
        zf = zn + 1000.0f;

    outNear = zn;
    outFar = zf;
}

inline float LinearizeReverseZDepthLH_ZO(float ndcDepth, float nearPlane, float farPlane, bool orthographic)
{
    if (orthographic)
        return farPlane - ndcDepth * (farPlane - nearPlane);

    return (nearPlane * farPlane) / (ndcDepth * (farPlane - nearPlane) + nearPlane);
}

} // namespace Engine::Renderer
} // namespace GameEngine
