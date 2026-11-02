#pragma once

#include "Mathematics/Matrix4x4.h"
#include "Mathematics/Vector2.h"
#include "Mathematics/Vector3.h"
#include "SplineGeometry/SplineVertex.h"
#include "Types/Types.h"

#include <vector>

namespace GameEngine::SplineGeometry
{
struct AreaGround;
}

namespace GameEngine::SplineRecipes
{

// A paved region's lid, ready for ToEngineMesh.
struct PavingLidMesh
{
    std::vector<SplineGeometry::SplineVertex> Vertices;
    std::vector<uint32> Indices;
    Mathematics::Vector3 MinBounds{};
    Mathematics::Vector3 MaxBounds{};
};

// The phase, in meters, that `seed` moves the paving's pattern on the ground.
Mathematics::Vector2 PavingPatternPhase(uint32 seed);

// The lid of `ground` in the space of the owner whose world matrix is `ownerWorld`: every
// triangle faces up, normals averaged over the triangles at a point, U and V in world meters
// along the owner's x and z axes on the ground plus `phase`.
PavingLidMesh BuildPavingLidMesh(const SplineGeometry::AreaGround& ground, const Mathematics::Matrix4x4& ownerWorld,
                               const Mathematics::Vector2& phase);

} // namespace GameEngine::SplineRecipes
