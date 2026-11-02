#include "UndoRedo/EntityComponentsEdit.h"

#include "ECS/ECSTemplates.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "EditorChangeNotifications.h"
#include "UndoRedo/IEditorCommand.h"
#include "UndoRedo/UndoRedoService.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace GameEngine::Editor
{

namespace
{

using ComponentBytes = std::vector<std::uint8_t>;

// One component the edit touched: its bytes before and after, absent on the side where the
// entity did not carry it.
struct ComponentChange
{
    ECS::ComponentTypeId TypeId = 0;
    std::optional<ComponentBytes> Before;
    std::optional<ComponentBytes> After;
};

// The components whose presence or bytes differ between two captures of one entity.
std::vector<ComponentChange> DiffEntityComponents(const EntityComponentBytes& before, const EntityComponentBytes& after)
{
    std::vector<ComponentChange> changes;
    auto b = before.begin();
    auto a = after.begin();
    while (b != before.end() || a != after.end())
    {
        if (a == after.end() || (b != before.end() && b->first < a->first))
        {
            changes.push_back({b->first, b->second, std::nullopt});
            ++b;
        }
        else if (b == before.end() || a->first < b->first)
        {
            changes.push_back({a->first, std::nullopt, a->second});
            ++a;
        }
        else
        {
            if (b->second != a->second)
                changes.push_back({a->first, b->second, a->second});
            ++b;
            ++a;
        }
    }
    return changes;
}

// Writes the `side` bytes of each change to `entity`, removing a component absent on that side, and
// announces each change as an UndoRedo change.
void ApplyComponentChanges(ECS::World& world, ECS::EntityHandle entity, const std::vector<ComponentChange>& changes,
                           std::optional<ComponentBytes> ComponentChange::*side, EditorChangeNotifications* notifications)
{
    if (!world.IsValid(entity))
        return;
    for (const ComponentChange& change : changes)
    {
        const std::optional<ComponentBytes>& bytes = change.*side;
        if (bytes)
            (void)world.ApplyComponentBytesImmediate(entity, change.TypeId, *bytes);
        else
            (void)world.RemoveComponentByTypeIdImmediate(entity, change.TypeId);
    }
    if (!notifications)
        return;
    for (const ComponentChange& change : changes)
        notifications->NotifyComponentChanged({&world, entity, change.TypeId, EditorChangeNotifications::ChangeKind::UndoRedo});
}

class EntityComponentsEditCommand final : public IEditorCommand
{
public:
    EntityComponentsEditCommand(std::string label, ECS::World& world, ECS::EntityHandle entity,
                                EditorChangeNotifications* notifications, std::vector<ComponentChange> changes)
        : m_Label(std::move(label)),
          m_World(world),
          m_Entity(entity),
          m_Notifications(notifications),
          m_Changes(std::move(changes))
    {
    }

    const char* GetName() const override { return m_Label.c_str(); }
    const char* GetTypeName() const override { return "EntityComponentsEditCommand"; }
    void Do() override { Apply(&ComponentChange::After); }
    void Undo() override { Apply(&ComponentChange::Before); }

private:
    void Apply(std::optional<ComponentBytes> ComponentChange::*side)
    {
        ApplyComponentChanges(m_World, m_Entity, m_Changes, side, m_Notifications);
    }

    std::string m_Label;
    ECS::World& m_World;
    ECS::EntityHandle m_Entity;
    EditorChangeNotifications* m_Notifications;
    std::vector<ComponentChange> m_Changes;
};

} // namespace

EntityComponentBytes CaptureEntityComponents(const ECS::World& world, ECS::EntityHandle entity)
{
    EntityComponentBytes components;
    const ECS::Archetype* archetype = world.GetEntityArchetype(entity);
    if (!archetype)
        return components;
    for (const ECS::ComponentTypeId typeId : archetype->GetSignature().GetComponents())
    {
        ComponentBytes bytes;
        if (world.CaptureComponentBytes(entity, typeId, bytes))
            components.emplace_back(typeId, std::move(bytes));
    }
    std::sort(components.begin(), components.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return components;
}

void RestoreEntityComponents(ECS::World& world, ECS::EntityHandle entity, const EntityComponentBytes& captured,
                             EditorChangeNotifications* notifications)
{
    const std::vector<ComponentChange> changes = DiffEntityComponents(CaptureEntityComponents(world, entity), captured);
    ApplyComponentChanges(world, entity, changes, &ComponentChange::After, notifications);
}

bool CommitEntityComponentsEdit(ECS::World& world, ECS::EntityHandle entity, UndoRedoService* undo,
                                EditorChangeNotifications* notifications, const std::string& label,
                                const std::function<bool()>& edit)
{
    if (!undo)
        return edit();
    const EntityComponentBytes before = CaptureEntityComponents(world, entity);
    const bool applied = edit();
    // Diffed whatever the edit answered: a write refused part-way has already landed what it
    // wrote before refusing, and the step is what lets the user take that back.
    std::vector<ComponentChange> changes = DiffEntityComponents(before, CaptureEntityComponents(world, entity));
    if (!changes.empty())
    {
        undo->CommitAlreadyApplied(
            std::make_unique<EntityComponentsEditCommand>(label, world, entity, notifications, std::move(changes)));
    }
    return applied;
}

} // namespace GameEngine::Editor
