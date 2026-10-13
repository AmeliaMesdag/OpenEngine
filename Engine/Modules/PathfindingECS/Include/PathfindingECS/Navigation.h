#pragma once

#include "Types/Types.h"

#include <string>

namespace GameEngine::ECS
{
class World;
struct EntityHandle;
} // namespace GameEngine::ECS

namespace GameEngine::Components
{
struct NavigationSurface;
} // namespace GameEngine::Components

namespace GameEngine::Mathematics
{
struct Vector3;
} // namespace GameEngine::Mathematics

namespace GameEngine::Navigation
{

/// Asks the NavigationSurface on `surface` to bake again from the current terrain, static collider
/// shapes and mesh renderers. The bake starts on the next update of NavigationSurfaceSystem; a mesh
/// keeps its current tiles until each new one is built. Returns false when the entity has no
/// NavigationSurface.
bool RequestBake(ECS::World& world, ECS::EntityHandle surface);

/// The surface's bake state as one line of text, for example "Baking: 212 of 1,024 tiles" or
/// "Baked: 1,024 tiles, 1,948 polygons, 0.5 MiB, version 12". A failure names what to change.
std::string FormatBakeStatus(const Components::NavigationSurface& surface);

/// The box, in world metres, the NavigationSurface on `surface` covers: its explicit bounds around
/// the entity, or, with Fit Bounds, the planar terrain's footprint, else everything the bake reads
/// (gathered, so not for every frame). False when the entity has no surface or the box is empty.
bool SurfaceBounds(ECS::World& world, ECS::EntityHandle surface, Mathematics::Vector3& outMin,
                   Mathematics::Vector3& outMax);

/// What a bake of `surface` over the box from `min` to `max` makes, as the bake counts it: mesh
/// tiles in Mesh mode, grid cells in Grid mode.
uint64 BakeUnitCount(const Components::NavigationSurface& surface, const Mathematics::Vector3& min,
                     const Mathematics::Vector3& max);

} // namespace GameEngine::Navigation
