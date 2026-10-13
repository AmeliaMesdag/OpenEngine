#include "NavigationGridBake.h"

#include "NavigationSourceCollector.h"

#include "Pathfinding/GridMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace GameEngine::PathfindingECS
{

namespace
{

constexpr float32 kNoGround = std::numeric_limits<float32>::lowest();
constexpr float32 kDegreesToRadians = 3.14159265358979323846f / 180.0f;
// Cells per side of a bucket the static triangles are sorted into, so a cell tests only the
// triangles near it.
constexpr uint32 kBucketCells = 8;

// The up-facing static triangles, sorted into square buckets over the grid's extent.
class TriangleBuckets
{
  public:
    TriangleBuckets(const Pathfinding::GridMap& grid, const NavigationTriangles& triangles)
        : m_Triangles(triangles)
    {
        const auto& settings = grid.GetSettings();
        float32 maxX = 0.0f, maxZ = 0.0f;
        grid.CellToWorld(settings.Width - 1, settings.Depth - 1, maxX, maxZ);
        m_MinX = settings.OriginX - settings.CellSize;
        m_MinZ = settings.OriginZ - settings.CellSize;
        m_BucketSize = settings.CellSize * static_cast<float32>(kBucketCells);
        m_BucketsX = static_cast<uint32>(std::ceil((maxX + settings.CellSize - m_MinX) / m_BucketSize)) + 1;
        m_BucketsZ = static_cast<uint32>(std::ceil((maxZ + settings.CellSize - m_MinZ) / m_BucketSize)) + 1;
        m_Buckets.resize(static_cast<size_t>(m_BucketsX) * m_BucketsZ);

        for (uint32 t = 0; t < triangles.TriangleCount(); ++t)
        {
            const float32* a = Vertex(t, 0);
            const float32* b = Vertex(t, 1);
            const float32* c = Vertex(t, 2);
            // Recast's convention: the cross of (b - a) and (c - a) points up for a walkable face.
            const float32 upward = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]);
            if (!(upward > 0.0f))
                continue;
            const uint32 x0 = BucketX(std::min({a[0], b[0], c[0]}));
            const uint32 x1 = BucketX(std::max({a[0], b[0], c[0]}));
            const uint32 z0 = BucketZ(std::min({a[2], b[2], c[2]}));
            const uint32 z1 = BucketZ(std::max({a[2], b[2], c[2]}));
            for (uint32 z = z0; z <= z1; ++z)
                for (uint32 x = x0; x <= x1; ++x)
                    m_Buckets[static_cast<size_t>(z) * m_BucketsX + x].push_back(t);
        }
    }

    // The highest up-facing triangle under (x, z), or kNoGround.
    float32 GroundAt(float32 x, float32 z) const
    {
        float32 ground = kNoGround;
        for (uint32 t : m_Buckets[static_cast<size_t>(BucketZ(z)) * m_BucketsX + BucketX(x)])
        {
            float32 height = 0.0f;
            if (HeightOnTriangle(t, x, z, height))
                ground = std::max(ground, height);
        }
        return ground;
    }

  private:
    const float32* Vertex(uint32 triangle, uint32 corner) const
    {
        return &m_Triangles.Vertices[static_cast<size_t>(m_Triangles.Indices[triangle * 3 + corner]) * 3];
    }

    uint32 BucketX(float32 x) const
    {
        const float32 cell = std::floor((x - m_MinX) / m_BucketSize);
        return static_cast<uint32>(std::clamp(cell, 0.0f, static_cast<float32>(m_BucketsX - 1)));
    }

    uint32 BucketZ(float32 z) const
    {
        const float32 cell = std::floor((z - m_MinZ) / m_BucketSize);
        return static_cast<uint32>(std::clamp(cell, 0.0f, static_cast<float32>(m_BucketsZ - 1)));
    }

    // The triangle's height above (x, z) when the point lies inside its XZ projection.
    bool HeightOnTriangle(uint32 triangle, float32 x, float32 z, float32& outHeight) const
    {
        const float32* a = Vertex(triangle, 0);
        const float32* b = Vertex(triangle, 1);
        const float32* c = Vertex(triangle, 2);
        const float32 det = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2]);
        if (std::abs(det) < 1.0e-12f)
            return false;
        const float32 wa = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / det;
        const float32 wb = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / det;
        const float32 wc = 1.0f - wa - wb;
        constexpr float32 kEdge = -1.0e-5f;
        if (wa < kEdge || wb < kEdge || wc < kEdge)
            return false;
        outHeight = wa * a[1] + wb * b[1] + wc * c[1];
        return true;
    }

    const NavigationTriangles& m_Triangles;
    float32 m_MinX = 0.0f;
    float32 m_MinZ = 0.0f;
    float32 m_BucketSize = 1.0f;
    uint32 m_BucketsX = 1;
    uint32 m_BucketsZ = 1;
    std::vector<std::vector<uint32>> m_Buckets;
};

// Blocks every walkable cell whose ground differs from a neighbour's by more than MaxSlope allows
// over the distance between their centres; per-neighbour distances keep diagonals and hex cells
// right. A neighbour with no ground has no height to compare.
void BlockSteepCells(Pathfinding::GridMap& grid, const std::vector<uint8>& hasGround)
{
    const auto& settings = grid.GetSettings();
    const float32 maxSlopeTangent = std::tan(settings.MaxSlope * kDegreesToRadians);
    for (uint32 cz = 0; cz < settings.Depth; ++cz)
    {
        for (uint32 cx = 0; cx < settings.Width; ++cx)
        {
            if (grid.IsCellBlocked(cx, cz))
                continue;
            const float32 height = grid.GetCellHeight(cx, cz);
            float32 cellX = 0.0f, cellZ = 0.0f;
            grid.CellToWorld(cx, cz, cellX, cellZ);

            uint32 neighbourX[8], neighbourZ[8];
            const uint32 neighbours = grid.GetNeighborCells(cx, cz, neighbourX, neighbourZ, 8);
            for (uint32 i = 0; i < neighbours; ++i)
            {
                if (!hasGround[grid.CellIndex(neighbourX[i], neighbourZ[i])])
                    continue;
                float32 x = 0.0f, z = 0.0f;
                grid.CellToWorld(neighbourX[i], neighbourZ[i], x, z);
                const float32 distance = std::sqrt((x - cellX) * (x - cellX) + (z - cellZ) * (z - cellZ));
                if (std::abs(grid.GetCellHeight(neighbourX[i], neighbourZ[i]) - height) > maxSlopeTangent * distance)
                {
                    grid.SetCellBlocked(cx, cz, true);
                    break;
                }
            }
        }
    }
}

} // namespace

std::unique_ptr<Pathfinding::GridMap> BakeNavigationGrid(const Pathfinding::GridSettings& settings,
                                                         const NavigationTerrainSource& terrain,
                                                         const NavigationStaticGeometry& geometry,
                                                         int32& outTerrainTileX, int32& outTerrainTileZ)
{
    auto grid = std::make_unique<Pathfinding::GridMap>(settings);
    const TriangleBuckets buckets(*grid, geometry.Triangles);
    std::vector<uint8> hasGround(grid->GetCellCount(), 0);

    for (uint32 cz = 0; cz < settings.Depth; ++cz)
    {
        for (uint32 cx = 0; cx < settings.Width; ++cx)
        {
            float32 x = 0.0f, z = 0.0f;
            grid->CellToWorld(cx, cz, x, z);
            float32 ground = buckets.GroundAt(x, z);
            if (terrain.Query.ContainsXZ(x, z))
            {
                float32 height = 0.0f;
                if (!terrain.Query.SampleHeight(x, z, height))
                {
                    if (!terrain.Query.TileCoordinateAt(x, z, outTerrainTileX, outTerrainTileZ))
                        outTerrainTileX = outTerrainTileZ = 0;
                    return nullptr;
                }
                ground = std::max(ground, height);
            }
            if (ground == kNoGround)
            {
                grid->SetCellBlocked(cx, cz, true);
                continue;
            }
            grid->SetCellHeight(cx, cz, ground);
            hasGround[grid->CellIndex(cx, cz)] = 1;
        }
    }
    BlockSteepCells(*grid, hasGround);
    return grid;
}

} // namespace GameEngine::PathfindingECS
