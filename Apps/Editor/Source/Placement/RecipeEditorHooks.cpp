#include "Placement/RecipeEditorHooks.h"

#include "Editor/Registries/EditorServiceRegistry.h"
#include "Placement/FenceSpanOverrideRemap.h"
#include "UndoRedo/Spline/SplineGroundEditCommand.h"
#include "UndoRedo/Spline/SplinePointEdits.h"
#include "UndoRedo/UndoRedoService.h"

#include "ECS/Systems.h"
#include "SplineRecipes/RecipeControllers.h"
#include "SplineRecipes/SplineRecipeSystem.h"

#include <memory>

namespace GameEngine::Editor
{

std::unique_ptr<IEditorService> RecipeEditorHooks::Create(const EditorServiceContext& context)
{
    auto* recipes = context.Systems.GetSystem<SplineRecipes::SplineRecipeSystem>();
    if (!recipes)
        return nullptr;
    return std::make_unique<RecipeEditorHooks>(context.Undo, *recipes);
}

RecipeEditorHooks::RecipeEditorHooks(UndoRedoService* undo, SplineRecipes::SplineRecipeSystem& recipes)
    : m_Undo(undo),
      m_Recipes(recipes),
      m_PointEditSubscription(SplinePointEdited().Subscribe(
          [&recipes](const SplinePointEdit& edit)
          { RemapFenceOverridesOnPointEdit(edit, recipes.GetControllers().FenceRunSpanCounts(edit.Entity)); }))
{
    m_Recipes.SetEditJournal(this);
}

RecipeEditorHooks::~RecipeEditorHooks()
{
    m_Recipes.SetEditJournal(nullptr);
}

void RecipeEditorHooks::RecordGroundEdit(ECS::World& world, const SplineRecipes::SplineGroundEdit& edit)
{
    if (!m_Undo)
        return;
    // The edit that caused the sweep is the newest entry: one undo reverts both. With no entry to
    // join, a sweep that removes a piece gets an entry of its own, so an undo puts the piece back.
    if (m_Undo->CanJoinTop())
        m_Undo->CommitJoiningTop(std::make_unique<SplineGroundEditCommand>(&world, edit));
    else if (edit.Removes())
        m_Undo->CommitAlreadyApplied(std::make_unique<SplineGroundEditCommand>(&world, edit));
}

void RecipeEditorHooks::ReleaseSceneScoped(ECS::World& world)
{
    m_Recipes.ReleaseSceneScoped(world);
}

} // namespace GameEngine::Editor
