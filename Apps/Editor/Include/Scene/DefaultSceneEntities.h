#pragma once

namespace GameEngine
{
namespace ECS { class World; }
namespace Engine::Renderer { class RenderServices; }

namespace Editor
{

// A new scene's sky: 10:00 local solar time at 40 degrees north, on the
// component's default day (the March equinox).
inline constexpr float kNewSceneTimeOfDayHours = 10.0f;
inline constexpr float kNewSceneLatitudeDeg = 40.0f;

// Seeds a freshly-cleared world with a basic set of entities so the scene
// view is immediately usable: directional light, sky environment, main camera,
// global post-process volume, ground plane, and two spheres (one with the
// mirror material that shows the environment it reflects). It seeds no
// reflection probe: a new scene's ambient and reflections come from the sky.
void SeedDefaultSceneEntities(ECS::World& world, Engine::Renderer::RenderServices* renderServices);

} // namespace Editor
} // namespace GameEngine
