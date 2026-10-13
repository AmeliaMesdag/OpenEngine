#pragma once

namespace GameEngine::Rendering
{

/// One night-sky star as sky_stars_billboard.vert reads it (std430, 32 bytes).
struct StarInstanceGPU
{
    /// xyz: unit direction in world space; w: brightness in [0.3, 1].
    float dirAndBrightness[4];
    /// x: size scale; y: rotation in radians; z: twinkle seed; w: colour temperature in [0, 1].
    float properties[4];
};

} // namespace GameEngine::Rendering
