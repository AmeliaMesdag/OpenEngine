#include "SplineRecipes/Paving/PavingMesh.h"

#include "SplineGeometry/AreaGround.h"
#include "SplineLayout/PieceEntity.h"

#include <algorithm>
#include <cmath>

namespace GameEngine::SplineRecipes
{
namespace
{

using Mathematics::Vector2;
using Mathematics::Vector3;

// The phase spans this many meters on each axis: past any material's tile.
constexpr float32 kPatternPhaseRangeMeters = 64.0f;
// The R2 sequence's steps (the plastic number's inverse powers) in 32-bit fixed point: the product
// with a seed wraps to the step's fractional part exactly, so every seed keeps its own phase.
constexpr uint32 kPhaseStepU = 0xC13FA9A9u;
constexpr uint32 kPhaseStepV = 0x91E10DA5u;

Vector2 GroundAxis(const Mathematics::Matrix4x4& world, int column, const Vector2& fallback)
{
    const Vector3 axis = world.TransformPoint(column == 0 ? Vector3(1, 0, 0) : Vector3(0, 0, 1)) -
                         world.TransformPoint(Vector3(0, 0, 0));
    const Vector2 flat(axis.x, axis.z);
    const float32 length = flat.Length();
    return length > 1.0e-6f ? flat / length : fallback;
}

} // namespace

Vector2 PavingPatternPhase(uint32 seed)
{
    const auto fraction = [](uint32 fixedPoint) { return std::ldexp(static_cast<float32>(fixedPoint), -32); };
    return Vector2(fraction(seed * kPhaseStepU), fraction(seed * kPhaseStepV)) * kPatternPhaseRangeMeters;
}

PavingLidMesh BuildPavingLidMesh(const SplineGeometry::AreaGround& ground, const Mathematics::Matrix4x4& ownerWorld,
                               const Vector2& phase)
{
    PavingLidMesh mesh;
    if (ground.LidPoints.empty() || ground.LidTriangles.size() < 3u)
        return mesh;
    const Mathematics::Matrix4x4 toLocal = SplineLayout::InvertPlacerWorld(ownerWorld.Data());
    const Vector2 uAxis = GroundAxis(ownerWorld, 0, Vector2(1.0f, 0.0f));
    const Vector2 vAxis = GroundAxis(ownerWorld, 2, Vector2(0.0f, 1.0f));

    // Every triangle wound so cross(b - a, c - a) points up, the sweep's rule.
    mesh.Indices.reserve(ground.LidTriangles.size());
    std::vector<Vector3> normals(ground.LidPoints.size(), Vector3(0.0f, 0.0f, 0.0f));
    for (size_t t = 0; t + 2 < ground.LidTriangles.size(); t += 3)
    {
        uint32 a = ground.LidTriangles[t];
        uint32 b = ground.LidTriangles[t + 1];
        uint32 c = ground.LidTriangles[t + 2];
        Vector3 face = Vector3::Cross(ground.LidPoints[b] - ground.LidPoints[a], ground.LidPoints[c] - ground.LidPoints[a]);
        if (face.y < 0.0f)
        {
            std::swap(b, c);
            face = face * -1.0f;
        }
        normals[a] = normals[a] + face;
        normals[b] = normals[b] + face;
        normals[c] = normals[c] + face;
        mesh.Indices.insert(mesh.Indices.end(), {a, b, c});
    }

    // Only the points a triangle uses become vertices: a ring sample the lid leaves out has none.
    constexpr uint32 kUnused = ~0u;
    std::vector<uint32> vertexOf(ground.LidPoints.size(), kUnused);
    std::vector<uint32> pointOf;
    for (uint32& index : mesh.Indices)
    {
        if (vertexOf[index] == kUnused)
        {
            vertexOf[index] = static_cast<uint32>(pointOf.size());
            pointOf.push_back(index);
        }
        index = vertexOf[index];
    }

    const Vector3 localOrigin = toLocal.TransformPoint(Vector3(0.0f, 0.0f, 0.0f));
    mesh.Vertices.resize(pointOf.size());
    for (size_t v = 0; v < pointOf.size(); ++v)
    {
        const size_t pointIndex = pointOf[v];
        const Vector3& point = ground.LidPoints[pointIndex];
        const float32 length = normals[pointIndex].Length();
        const Vector3 normal = length > 0.0f ? normals[pointIndex] / length : Vector3(0.0f, 1.0f, 0.0f);
        // The tangent is the U axis on the surface: the ground axis lifted onto the normal's plane.
        const Vector3 flatU(uAxis.x, 0.0f, uAxis.y);
        Vector3 tangent = flatU - normal * Vector3::Dot(flatU, normal);
        tangent = tangent.Length() > 1.0e-6f ? tangent.Normalize() : flatU;

        SplineGeometry::SplineVertex& vertex = mesh.Vertices[v];
        vertex.Position = toLocal.TransformPoint(point);
        vertex.Normal = (toLocal.TransformPoint(normal) - localOrigin).NormalizeOrZero();
        const Vector3 localTangent = (toLocal.TransformPoint(tangent) - localOrigin).NormalizeOrZero();
        vertex.Tangent = Mathematics::Vector4(localTangent.x, localTangent.y, localTangent.z, 1.0f);
        const Vector2 xz(point.x, point.z);
        vertex.UV = Vector2(Vector2::Dot(xz, uAxis), Vector2::Dot(xz, vAxis)) + phase;
        if (v == 0)
        {
            mesh.MinBounds = vertex.Position;
            mesh.MaxBounds = vertex.Position;
        }
        mesh.MinBounds = Vector3(std::min(mesh.MinBounds.x, vertex.Position.x), std::min(mesh.MinBounds.y, vertex.Position.y),
                                 std::min(mesh.MinBounds.z, vertex.Position.z));
        mesh.MaxBounds = Vector3(std::max(mesh.MaxBounds.x, vertex.Position.x), std::max(mesh.MaxBounds.y, vertex.Position.y),
                                 std::max(mesh.MaxBounds.z, vertex.Position.z));
    }
    return mesh;
}

} // namespace GameEngine::SplineRecipes
