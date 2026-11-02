#include <algorithm>
#include <cstring>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "Components/Hierarchy.h"
#include "Components/Name.h"
#include "Components/RuntimeOnlyEntity.h"
#include "ECS/ECSTemplates.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "Editor/Hierarchy/HierarchyDataProvider.h"
#include "Editor/Hierarchy/HierarchyReparentRules.h"
#include "InspectorLayoutFixture.h"
#include "Inspectors/GeneratedEntityNotice.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/InspectorNotice.h"
#include "UI/Controls/Label.h"
#include "UI/UIElement.h"
#include "UI/UIEvents.h"

using GameEngine::Button;
using GameEngine::HierarchyDataProvider;
using GameEngine::TreeId;
using GameEngine::kEventButtonClick;
using GameEngine::Label;
using GameEngine::UIElement;
using GameEngine::UIEvent;
using GameEngine::EditorUI::InspectorNotice;
using InspectorLayoutTesting::FindByClass;
using GameEngine::Components::Name;
using GameEngine::Components::Parent;
using GameEngine::Components::RuntimeOnlyEntity;
using GameEngine::ECS::Entity;
using GameEngine::ECS::EntityHandle;
using GameEngine::ECS::World;

using GameEngine::Editor::AddGeneratedEntityNotice;
using GameEngine::Editor::CanAcceptAuthoredChildren;

namespace
{

Name MakeName(const char* text)
{
    Name n{};
    std::strncpy(n.value, text, sizeof(n.value) - 1);
    return n;
}

// The notice the host was given, or null when nothing was added.
InspectorNotice* NoticeIn(const UIElement& parent)
{
    for (const auto& child : parent.GetChildren())
    {
        if (auto* notice = dynamic_cast<InspectorNotice*>(child.get()))
            return notice;
    }
    return nullptr;
}

// The notice's text, or "" when nothing was added.
std::string NoticeTextIn(const UIElement& parent)
{
    InspectorNotice* notice = NoticeIn(parent);
    const auto* label = notice ? dynamic_cast<const Label*>(FindByClass(notice, "inspector-notice-text")) : nullptr;
    return label ? label->GetText() : std::string{};
}

// The notice's action while it is shown, or null.
Button* ShownActionIn(const UIElement& parent)
{
    InspectorNotice* notice = NoticeIn(parent);
    auto* action = notice ? dynamic_cast<Button*>(FindByClass(notice, "inspector-notice-action")) : nullptr;
    return action && !action->HasClass("hidden") ? action : nullptr;
}

void Click(Button& button)
{
    UIEvent e{};
    e.Id = kEventButtonClick;
    e.Target = &button;
    e.CurrentTarget = &button;
    button.DispatchEvent(e);
}

// A paved recipe with an authored bed and a road with no authored children, as the
// Hierarchy shows them before any generator runs.
struct HierarchyScene
{
    World World;
    EntityHandle Square;
    EntityHandle Bed;
    EntityHandle Road;

    HierarchyScene()
    {
        Entity square = World.Create();
        square.Set(MakeName("Market Square"));
        Entity bed = World.Create();
        bed.Set(MakeName("Bed 1"));
        bed.Set(Parent{square.GetHandle()});
        Entity road = World.Create();
        road.Set(MakeName("High Street"));
        World.ProcessCommands();
        Square = square.GetHandle();
        Bed = bed.GetHandle();
        Road = road.GetHandle();
    }

    // One generated section under each listed entity, as a spline recipe spawns its chunks.
    // Returns the road's section.
    EntityHandle AddGeneratedSections()
    {
        EntityHandle roadSection{};
        for (const EntityHandle owner : {Square, Road})
        {
            Entity section = World.Create();
            section.Set(MakeName("Section 0"));
            section.Set(Parent{owner});
            section.Set(RuntimeOnlyEntity{});
            roadSection = section.GetHandle();
        }
        World.ProcessCommands();
        return roadSection;
    }
};

// The rows the provider lists: the two owners at the top, the bed alone under the square,
// nothing under the road and no expander on it.
void ExpectOnlyAuthoredRows(const HierarchyDataProvider& rows, const HierarchyScene& scene)
{
    const TreeId square = HierarchyDataProvider::Encode(scene.Square);
    const TreeId road = HierarchyDataProvider::Encode(scene.Road);
    ASSERT_EQ(rows.GetChildCount(HierarchyDataProvider::kRootId), 2);
    EXPECT_EQ(rows.GetChildCount(square), 1);
    EXPECT_EQ(rows.GetChildId(square, 0), HierarchyDataProvider::Encode(scene.Bed));
    EXPECT_TRUE(rows.IsExpandable(square));
    EXPECT_EQ(rows.GetChildCount(road), 0) << "a generated section has a row under its road";
    EXPECT_FALSE(rows.IsExpandable(road)) << "a road with only generated children has an expander";
}

} // namespace

// The drop rule that stops the blocker at its source. A generated entity is not
// authorable content, and a child parented under one serializes a parent= naming
// no entity in the saved file.
TEST(GeneratedEntityGuard, GeneratedEntityRefusesAuthoredChildren)
{
    World world;

    Entity authored = world.Create();
    Entity generated = world.Create();
    generated.Set(RuntimeOnlyEntity{});
    world.ProcessCommands();

    EXPECT_FALSE(CanAcceptAuthoredChildren(world, generated.GetHandle()));
    EXPECT_TRUE(CanAcceptAuthoredChildren(world, authored.GetHandle()));
    // An invalid handle is the scene root, which always accepts children — the
    // rule must not turn a drop onto empty space into a rejection.
    EXPECT_TRUE(CanAcceptAuthoredChildren(world, EntityHandle{}));
}

// Selecting a piece has to answer both halves of "does my edit survive?" — the
// scene does not keep it, and the next rebuild replaces it — and name what to
// edit instead.
TEST(GeneratedEntityGuard, NoticeNamesThePlacerAndBothHalvesOfTheAnswer)
{
    World world;

    Entity placer = world.Create();
    placer.Set(MakeName("Garden Path"));
    Entity piece = world.Create();
    piece.Set(RuntimeOnlyEntity{});
    piece.Set(Parent{placer.GetHandle()});
    world.ProcessCommands();

    UIElement host;
    AddGeneratedEntityNotice(&host, world, piece.GetHandle(), {});

    const std::string text = NoticeTextIn(host);
    ASSERT_FALSE(text.empty()) << "no notice was added";
    EXPECT_NE(text.find("Garden Path"), std::string::npos) << text;
    EXPECT_NE(text.find("not saved"), std::string::npos) << text;
    EXPECT_NE(text.find("overwritten"), std::string::npos) << text;
}

// Runtime-only entities with no parent exist too (HLOD proxies, terrain collider
// tiles). They get the honest short form rather than a dangling "Generated by".
TEST(GeneratedEntityGuard, ParentlessGeneratedEntityStillGetsANotice)
{
    World world;

    Entity proxy = world.Create();
    proxy.Set(RuntimeOnlyEntity{});
    world.ProcessCommands();

    UIElement host;
    AddGeneratedEntityNotice(&host, world, proxy.GetHandle(), [](EntityHandle) {});

    const std::string text = NoticeTextIn(host);
    ASSERT_FALSE(text.empty());
    EXPECT_EQ(text.find("Generated by"), std::string::npos) << text;
    EXPECT_NE(text.find("not saved"), std::string::npos) << text;
    EXPECT_NE(text.find("overwritten"), std::string::npos) << text;
    EXPECT_EQ(ShownActionIn(host), nullptr) << "there is no owner to select";
}

// The notice says to edit the owner instead, so it offers to take the user there: its action
// selects the owner through the inspector's SelectEntity, as clicking it in the Hierarchy would.
TEST(GeneratedEntityGuard, NoticeSelectsTheOwner)
{
    World world;

    Entity placer = world.Create();
    placer.Set(MakeName("Garden Path"));
    Entity piece = world.Create();
    piece.Set(RuntimeOnlyEntity{});
    piece.Set(Parent{placer.GetHandle()});
    world.ProcessCommands();

    EntityHandle selected{};
    UIElement host;
    AddGeneratedEntityNotice(&host, world, piece.GetHandle(),
                             [&selected](EntityHandle entity) { selected = entity; });

    Button* action = ShownActionIn(host);
    ASSERT_NE(action, nullptr) << "the notice offers no way to the owner it names";
    EXPECT_EQ(action->GetText(), "Select owner");
    EXPECT_NE(action->GetTooltip().find("Garden Path"), std::string::npos) << action->GetTooltip();

    Click(*action);
    EXPECT_EQ(selected.id, placer.GetHandle().id) << "the action selected something other than the owner";
}

// An authored entity must get no notice at all — a banner on ordinary content
// would teach the user to ignore it.
TEST(GeneratedEntityGuard, AuthoredEntityGetsNoNotice)
{
    World world;

    Entity authored = world.Create();
    authored.Set(MakeName("Lamp Post"));
    world.ProcessCommands();

    UIElement host;
    AddGeneratedEntityNotice(&host, world, authored.GetHandle(), {});

    EXPECT_TRUE(host.GetChildren().empty());
}

// The Hierarchy lists authored content only: a full rebuild makes no row for a generated
// entity, and its owner's row and expander are those of the owner without it.
TEST(GeneratedEntityGuard, HierarchyRebuildListsNoGeneratedEntity)
{
    HierarchyScene scene;
    scene.AddGeneratedSections();

    HierarchyDataProvider rows(nullptr);
    rows.SetWorld(&scene.World);

    ExpectOnlyAuthoredRows(rows, scene);
}

// A generated entity created after the rows are built makes no row on the incremental insert.
TEST(GeneratedEntityGuard, HierarchyIncrementalInsertListsNoGeneratedEntity)
{
    HierarchyScene scene;
    HierarchyDataProvider rows(nullptr);
    rows.SetWorld(&scene.World);
    ExpectOnlyAuthoredRows(rows, scene);

    scene.AddGeneratedSections();
    const auto diff = rows.ApplyIdentityDiff();

    ASSERT_FALSE(diff.Escalate) << "the insert fell back to a full rebuild";
    ExpectOnlyAuthoredRows(rows, scene);
}

// A Scene View pick of a generated entity shows on the row of its owner; a generated entity
// with no listed owner has no row to show it.
TEST(GeneratedEntityGuard, HierarchyShowsAGeneratedEntityOnItsOwnersRow)
{
    HierarchyScene scene;
    const EntityHandle section = scene.AddGeneratedSections();
    Entity proxy = scene.World.Create();
    proxy.Set(RuntimeOnlyEntity{});
    scene.World.ProcessCommands();

    EXPECT_EQ(HierarchyDataProvider::ListedSelfOrAncestor(scene.World, section), scene.Road);
    EXPECT_EQ(HierarchyDataProvider::ListedSelfOrAncestor(scene.World, scene.Bed), scene.Bed);
    EXPECT_FALSE(HierarchyDataProvider::ListedSelfOrAncestor(scene.World, proxy.GetHandle()).IsValid());
}

// A multi-pick of two generated sections of one road selects the road's row once, beside the
// authored pick; the anchor is the row of the first pick.
TEST(GeneratedEntityGuard, HierarchyMultiPickSelectsTheOwnerOnce)
{
    HierarchyScene scene;
    const EntityHandle first = scene.AddGeneratedSections();
    const EntityHandle second = scene.AddGeneratedSections();
    const std::vector<EntityHandle> picks{first, scene.Bed, second};

    std::vector<TreeId> ids;
    const TreeId anchor = HierarchyDataProvider::ListedRowIds(scene.World, picks, ids);

    const TreeId road = HierarchyDataProvider::Encode(scene.Road);
    const TreeId bed = HierarchyDataProvider::Encode(scene.Bed);
    EXPECT_EQ(anchor, road);
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(std::count(ids.begin(), ids.end(), road), 1);
    EXPECT_EQ(std::count(ids.begin(), ids.end(), bed), 1);
}
