#pragma once

#include "Rendering/Sky/StarCatalog.h"

#include <cstdint>
#include <vector>

namespace GameEngine::Rendering
{

/// How far a star's sparkle turns off the screen's axes, either way, in radians. Wide enough that
/// the sky does not read as one stamp repeated, narrow enough that no small star's points land on
/// the pixel grid's diagonals, where a four-pointed star a few pixels across reads as a square.
inline constexpr float kStarRotationSpreadRadians = 0.6f;

/// The sky's star catalog: starCount stars spread uniformly over the sphere from a fixed seed, so
/// every run draws the same sky.
std::vector<StarInstanceGPU> BuildStarCatalog(uint32_t starCount);

} // namespace GameEngine::Rendering
