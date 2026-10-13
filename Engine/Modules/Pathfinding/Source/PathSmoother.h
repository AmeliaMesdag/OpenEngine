#pragma once

#include "Pathfinding/GridMap.h"
#include "Pathfinding/PathfindingTypes.h"

#include <vector>

namespace GameEngine::Pathfinding
{

// Simplifies a grid path by removing intermediate waypoints where line-of-sight
// exists between non-adjacent points. Uses the view's Raycast to verify visibility.
class PathSmoother
{
public:
    // Smooths the path in-place. Returns the new point count.
    // The input points are world-space coordinates from GridMap::FindPath.
    static uint32 Smooth(const GridMap::ReadView& grid, std::vector<PathPoint>& points);
};

} // namespace GameEngine::Pathfinding
