// Draw order of the world forward pass. A blended draw writes no depth, so every
// depth-writing draw records before every blended draw.

#pragma once

#include "Engine/Rendering/DrawCommand.h"
#include "Engine/Rendering/Material.h"
#include "Engine/Rendering/WorldDrawBuilder.h"
#include "Rendering/Materials/MaterialAlphaMode.h"

#include <concepts>
#include <span>

namespace GameEngine::Engine::Renderer
{

inline bool IsBlendedMaterial(const Material* material)
{
    return material && material->GetAlphaMode() == MaterialAlphaMode::Blend;
}

// Calls the recorders for every depth-writing entity and contributor, then for
// every blended entity and contributor. The order of each source stays the same
// in each phase. The recorders apply their own draw guards.
template <typename RecordEntity, typename RecordContributor>
    requires std::invocable<const RecordEntity&, const WorldDrawBuilder::BatchKey&>
        && std::invocable<const RecordContributor&, const DrawCommand&>
void DrainWorldForwardOpaqueFirst(std::span<const WorldDrawBuilder::BatchKey> entities,
                                 std::span<const DrawCommand> contributors,
                                 const RecordEntity& recordEntity,
                                 const RecordContributor& recordContributor)
{
    for (bool blendPhase : {false, true})
    {
        for (const auto& key : entities)
            if (IsBlendedMaterial(key.material) == blendPhase)
                recordEntity(key);
        for (const auto& command : contributors)
            if (IsBlendedMaterial(command.Material) == blendPhase)
                recordContributor(command);
    }
}

} // namespace GameEngine::Engine::Renderer
