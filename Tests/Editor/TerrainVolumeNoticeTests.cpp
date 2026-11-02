// The Inspector notices for TerrainModifierVolume's Shape::Global.
//
// Shape::Global makes several fields stop meaning what the widget beside them
// implies, and the notices are the only thing that says so. They are therefore
// load-bearing UI, not decoration, and each one here is pinned against the
// specific wrong statement it replaced:
//
//   * "at full strength" contradicted the volume's own Weight, which still
//     multiplies into every effect and whose row sits directly beneath.
//   * The transform rows stay live and editable while the region stops being
//     scoped by them, so a drag changes nothing visible.
//   * The Stamp notice fired on component PRESENCE while the bake refuses on
//     ENABLED, so parking an effect the correct way produced a warning naming a
//     cause that was not the reason.
//
// Copy is asserted by phrase, not by whole string, so rewording stays cheap
// while the CLAIMS stay pinned.

#include <gtest/gtest.h>
#include <functional>
#include <string>
#include <vector>

#include "Components/Terrain/Terrain.h"
#include "Components/Terrain/TerrainModifierEffects.h"
#include "Components/Terrain/TerrainModifierVolume.h"
#include "ECS/ECSTemplates.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "InspectorRegistry.h"
#include "Terrain/TerrainVolumeNotices.h"
#include "TerrainECS/TerrainGpuBake.h"
#include "TerrainECS/TerrainService.h"
#include "UI/Controls/InspectorNotice.h"
#include "UI/Controls/Label.h"
#include "UI/UIElement.h"

using GameEngine::Label;
using GameEngine::UIElement;
using GameEngine::ECS::Entity;
using GameEngine::ECS::World;
using GameEngine::Editor::AddGlobalVolumeScopeNotice;
using GameEngine::Editor::AddStampInGlobalVolumeNotice;

namespace Components = GameEngine::Components;

namespace
{

// The whole subtree, not the host's own row: an info card is a control that owns
// the elements its copy lives on, so what a notice added is not always a direct
// child of the host. Both readings below walk it through here, so neither can
// miss a set of elements the other sees.
template <typename Fn>
void ForEachDescendant(const UIElement& parent, Fn& fn)
{
    for (const auto& child : parent.GetChildren())
    {
        if (!child)
            continue;
        fn(*child);
        ForEachDescendant(*child, fn);
    }
}

// Every label the notice added, warnings and plain lines alike, lowercased so a
// phrase assertion does not also pin capitalization.
std::string AllTextIn(const UIElement& parent)
{
    std::string all;
    auto collect = [&all](const UIElement& element)
    {
        if (const auto* label = dynamic_cast<const Label*>(&element))
        {
            all += label->GetText();
            all += "\n";
        }
    };
    ForEachDescendant(parent, collect);
    for (auto& c : all)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return all;
}

// The notices a user sees: a hidden notice is not counted.
std::size_t WarningCountIn(const UIElement& parent)
{
    std::size_t n = 0;
    auto count = [&n](const UIElement& element)
    {
        if (dynamic_cast<const GameEngine::EditorUI::InspectorNotice*>(&element) && !element.HasClass("hidden"))
            ++n;
    };
    ForEachDescendant(parent, count);
    return n;
}

bool Contains(const std::string& haystack, const char* needle)
{
    return haystack.find(needle) != std::string::npos;
}

Entity MakeVolume(World& world, Components::TerrainVolumeShape shape, float weight = 1.0f)
{
    Entity e = world.Create();
    Components::TerrainModifierVolume vol{};
    vol.Shape = shape;
    vol.Weight = weight;
    e.Set(vol);
    world.ProcessCommands();
    return e;
}

void AddStamp(World& world, Entity e, bool enabled)
{
    Components::TerrainStampEffect stamp{};
    stamp.Enabled = enabled;
    e.Set(stamp);
    world.ProcessCommands();
}

} // namespace

// ---- Scope notice ----------------------------------------------------------

// The notice has to carry BOTH things the shape takes away. The falloff rows are
// hidden for a global volume, so "no edge" is the only place that fact survives;
// the transform rows are NOT hidden, so without the second sentence a drag looks
// broken rather than inapplicable.
TEST(TerrainVolumeNotice, GlobalScopeNoticeNamesTheEdgeAndTheTransform)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);

    UIElement host;
    AddGlobalVolumeScopeNotice(&host, world, e.GetHandle(), /*anySphericalTerrain=*/false);

    const std::string text = AllTextIn(host);
    ASSERT_FALSE(text.empty()) << "no scope notice was added for a global volume";
    EXPECT_TRUE(Contains(text, "every terrain")) << text;
    EXPECT_TRUE(Contains(text, "no edge")) << text;
    EXPECT_TRUE(Contains(text, "no falloff")) << text;
    EXPECT_TRUE(Contains(text, "moving or resizing")) << text;
}

// The regression this test exists for: the notice said "at full strength" while
// ComputeWeight returns the volume's Weight for a global, and the Weight row sits
// directly beneath it. The notice must make NO strength claim at all — which also
// means its wording cannot depend on Weight.
TEST(TerrainVolumeNotice, GlobalScopeNoticeMakesNoStrengthClaimAtAnyWeight)
{
    for (const float weight : {1.0f, 0.5f, 0.0f})
    {
        World world;
        Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global, weight);

        UIElement host;
        AddGlobalVolumeScopeNotice(&host, world, e.GetHandle(), /*anySphericalTerrain=*/false);

        const std::string text = AllTextIn(host);
        EXPECT_FALSE(Contains(text, "full strength"))
            << "weight " << weight << " still claims full strength: " << text;
        EXPECT_FALSE(Contains(text, "at full")) << text;
    }
}

// The region copy is a property of the SHAPE, so it must read identically
// whatever the master weight is. Pins the invariant the test above only implies.
TEST(TerrainVolumeNotice, GlobalScopeNoticeIsIdenticalAcrossWeights)
{
    auto textAtWeight = [](float weight) {
        World world;
        Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global, weight);
        UIElement host;
        AddGlobalVolumeScopeNotice(&host, world, e.GetHandle(), /*anySphericalTerrain=*/false);
        return AllTextIn(host);
    };

    EXPECT_EQ(textAtWeight(1.0f), textAtWeight(0.35f));
}

// The notice must not describe the region as unaffected by POSITION outright: a
// Flatten in "Relative To Volume" mode still reads the entity's Y through
// ComputeVolumeReferenceHeight. The claim is about what the volume COVERS.
TEST(TerrainVolumeNotice, GlobalScopeNoticeDoesNotClaimPositionIsIgnored)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);

    UIElement host;
    AddGlobalVolumeScopeNotice(&host, world, e.GetHandle(), /*anySphericalTerrain=*/false);

    const std::string text = AllTextIn(host);
    EXPECT_FALSE(Contains(text, "position is ignored")) << text;
    EXPECT_FALSE(Contains(text, "position has no effect")) << text;
}

TEST(TerrainVolumeNotice, PlanetCaveatAppearsOnlyWithASphericalTerrain)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);

    UIElement without;
    AddGlobalVolumeScopeNotice(&without, world, e.GetHandle(), /*anySphericalTerrain=*/false);
    EXPECT_FALSE(Contains(AllTextIn(without), "spherical"));

    UIElement with;
    AddGlobalVolumeScopeNotice(&with, world, e.GetHandle(), /*anySphericalTerrain=*/true);
    EXPECT_TRUE(Contains(AllTextIn(with), "spherical"));
}

// Every other shape has a real footprint, so none of this applies to it.
TEST(TerrainVolumeNotice, NonGlobalShapesGetNoScopeNotice)
{
    for (const auto shape : {Components::TerrainVolumeShape::Rectangle,
                             Components::TerrainVolumeShape::Circle,
                             Components::TerrainVolumeShape::SplinePath,
                             Components::TerrainVolumeShape::SplineArea})
    {
        World world;
        Entity e = MakeVolume(world, shape);

        UIElement host;
        AddGlobalVolumeScopeNotice(&host, world, e.GetHandle(), /*anySphericalTerrain=*/true);
        EXPECT_TRUE(AllTextIn(host).empty())
            << "shape " << static_cast<int>(shape) << " got a global-scope notice";
    }
}

// ---- Stamp refusal notice --------------------------------------------------

TEST(TerrainVolumeNotice, EnabledStampInAGlobalVolumeIsFlagged)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);
    AddStamp(world, e, /*enabled=*/true);

    UIElement host;
    AddStampInGlobalVolumeNotice(&host, world, e.GetHandle());

    const std::string text = AllTextIn(host);
    ASSERT_FALSE(text.empty()) << "an enabled stamp on a global volume was not flagged";
    EXPECT_EQ(WarningCountIn(host), 1u);
    EXPECT_TRUE(Contains(text, "ignored")) << text;
    // Names the cause and BOTH ways out, not just the diagnosis.
    EXPECT_TRUE(Contains(text, "footprint")) << text;
    EXPECT_TRUE(Contains(text, "circle or rectangle")) << text;
    EXPECT_TRUE(Contains(text, "height offset")) << text;
}

// The regression this test exists for. The gather takes a stamp only when it is
// Enabled, so a DISABLED stamp is never refused and never warned about by the
// engine. Flagging it here would name a cause that is not why it is inactive,
// and prescribe a shape change that is not the fix — the effect's own Enabled
// toggle already says everything true about that state.
TEST(TerrainVolumeNotice, DisabledStampInAGlobalVolumeIsNotFlagged)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);
    AddStamp(world, e, /*enabled=*/false);

    UIElement host;
    AddStampInGlobalVolumeNotice(&host, world, e.GetHandle());

    EXPECT_TRUE(AllTextIn(host).empty())
        << "a disabled stamp was flagged as ignored-because-Global; it is inactive because it "
           "is disabled, and re-enabling it is what the notice should then answer";
}

// The complement, so the test above cannot pass by the notice never firing:
// re-enabling the same stamp on the same entity brings the notice back.
TEST(TerrainVolumeNotice, ReEnablingTheStampBringsTheNoticeBack)
{
    World world;
    Entity e = MakeVolume(world, Components::TerrainVolumeShape::Global);
    AddStamp(world, e, /*enabled=*/false);

    UIElement off;
    AddStampInGlobalVolumeNotice(&off, world, e.GetHandle());
    ASSERT_TRUE(AllTextIn(off).empty());

    auto* stamp = world.GetComponentForWrite<Components::TerrainStampEffect>(e.GetHandle());
    ASSERT_NE(stamp, nullptr);
    stamp->Enabled = true;

    UIElement on;
    AddStampInGlobalVolumeNotice(&on, world, e.GetHandle());
    EXPECT_FALSE(AllTextIn(on).empty())
        << "the notice did not return when the stamp was re-enabled";
}

// A stamp is only refused because the volume is global; on any shape with a
// footprint it bakes, so there is nothing to say.
TEST(TerrainVolumeNotice, StampInANonGlobalVolumeIsNotFlagged)
{
    for (const auto shape : {Components::TerrainVolumeShape::Rectangle,
                             Components::TerrainVolumeShape::Circle,
                             Components::TerrainVolumeShape::SplineArea})
    {
        World world;
        Entity e = MakeVolume(world, shape);
        AddStamp(world, e, /*enabled=*/true);

        UIElement host;
        AddStampInGlobalVolumeNotice(&host, world, e.GetHandle());
        EXPECT_TRUE(AllTextIn(host).empty())
            << "shape " << static_cast<int>(shape) << " flagged a stamp that bakes fine";
    }
}

// An effect entity with no volume at all is the orphan case, which has its own
// notice. This one must stay quiet rather than blaming a shape that is absent.
TEST(TerrainVolumeNotice, StampWithNoVolumeIsNotFlagged)
{
    World world;
    Entity e = world.Create();
    Components::TerrainStampEffect stamp{};
    e.Set(stamp);
    world.ProcessCommands();

    UIElement host;
    AddStampInGlobalVolumeNotice(&host, world, e.GetHandle());
    EXPECT_TRUE(AllTextIn(host).empty());
}

namespace
{
using GameEngine::Editor::AddCpuHeightBakeNotice;
using GameEngine::TerrainECS::GpuHeightBakeRefusal;
using GameEngine::TerrainECS::TerrainHeightBakeReport;
using Reason = GameEngine::TerrainECS::GpuHeightBakeRefusalReason;
namespace TerrainECS = GameEngine::TerrainECS;

// A tiled terrain entity and a volume. The test writes the terrain's bake report as
// TerrainModifierSystem does: a new decision increases the revision.
class TerrainSlowEditNotice : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_OwnsService = !TerrainECS::TerrainService::IsInitialized();
        if (m_OwnsService)
            TerrainECS::TerrainService::Initialize();
        TerrainECS::TiledTerrainConfig config{};
        config.WorldSizeX = 1024.0f;
        config.WorldSizeZ = 1024.0f;
        m_Handle = TerrainECS::TerrainService::Get().CreateTiledTerrain(config);
        Components::Terrain terrain{};
        terrain.TiledTerrainHandle = m_Handle.Index;
        terrain.TiledTerrainGeneration = m_Handle.Generation;
        Entity terrainEntity = m_World.Create();
        terrainEntity.Set(terrain);
        m_World.ProcessCommands();
        m_Volume = MakeVolume(m_World, Components::TerrainVolumeShape::Circle).GetHandle();
        m_OtherVolume = MakeVolume(m_World, Components::TerrainVolumeShape::Circle).GetHandle();
    }

    void TearDown() override
    {
        TerrainECS::TerrainService::Get().DestroyTiledTerrain(m_Handle);
        if (m_OwnsService)
            TerrainECS::TerrainService::Shutdown();
    }

    TerrainHeightBakeReport& Report()
    {
        return TerrainECS::TerrainService::Get().GetTiledTerrainData(m_Handle)->HeightBakeReport;
    }

    void Decide(Reason reason, GameEngine::ECS::EntityHandle volume, uint32_t otherRefusals = 0)
    {
        TerrainHeightBakeReport& report = Report();
        report.Path = reason == Reason::None ? TerrainECS::TerrainHeightBakePath::Gpu
                                             : TerrainECS::TerrainHeightBakePath::Cpu;
        report.ModifierRefusal = GpuHeightBakeRefusal{reason, volume};
        report.OtherModifierRefusals = otherRefusals;
        ++report.Revision;
    }

    // The inspector row of `volume`, built under `host`.
    void BuildRow(UIElement& host, GameEngine::ECS::EntityHandle volume, Reason rowCause)
    {
        GameEngine::InspectorContext ctx;
        ctx.Parent = &host;
        ctx.World = &m_World;
        ctx.Entity = volume;
        ctx.SimulationRefreshCallbacks = &m_Refresh;
        AddCpuHeightBakeNotice(ctx, rowCause);
    }

    // The text of the notice the row shows, or empty when it shows none.
    std::string RowText(GameEngine::ECS::EntityHandle volume, Reason rowCause)
    {
        UIElement host;
        BuildRow(host, volume, rowCause);
        m_Refresh.clear();
        return WarningCountIn(host) == 0 ? std::string{} : AllTextIn(host);
    }

    void RunRefresh()
    {
        for (auto& callback : m_Refresh)
            callback();
    }

    World m_World;
    GameEngine::ECS::EntityHandle m_Volume{};
    GameEngine::ECS::EntityHandle m_OtherVolume{};
    std::vector<std::function<void()>> m_Refresh;

private:
    bool m_OwnsService = false;
    TerrainECS::TiledTerrainHandle m_Handle{};
};
} // namespace

// The notice reads the bake's refusal, not the row's blend: a recipe road holds an Average flatten, but
// its spline shape is the refusal, and the recipe's own pieces show nothing on any row.
TEST_F(TerrainSlowEditNotice, ARecipeRoadShowsNoSlowEditNotice)
{
    Decide(Reason::SplineShape, m_Volume);
    for (const Reason rowCause : {Reason::None, Reason::PooledBlend, Reason::Stamp, Reason::Erosion})
        EXPECT_TRUE(RowText(m_Volume, rowCause).empty());
    for (const Reason refusal : {Reason::HeightFromVolume, Reason::GroundClaim})
    {
        Decide(refusal, m_Volume);
        EXPECT_TRUE(RowText(m_Volume, Reason::PooledBlend).empty());
    }
}

// An Average row shows the notice only on the volume the bake names. The words say what the user sees
// and what to change, the notice is information, and it counts the other effects that keep the CPU.
TEST_F(TerrainSlowEditNotice, APooledCircleShowsTheSlowEditNoticeOnItsAverageRow)
{
    Decide(Reason::PooledBlend, m_Volume);
    UIElement host;
    BuildRow(host, m_Volume, Reason::PooledBlend);
    const std::string text = AllTextIn(host);
    EXPECT_TRUE(Contains(text, "show more slowly")) << text;
    EXPECT_TRUE(Contains(text, "pick another blend")) << text;
    EXPECT_FALSE(Contains(text, "gpu")) << "the notice names an engine part: " << text;
    EXPECT_FALSE(Contains(text, "other effect")) << text;
    ASSERT_EQ(WarningCountIn(host), 1u);
    bool information = false;
    auto findKind = [&information](const UIElement& element)
    {
        if (dynamic_cast<const GameEngine::EditorUI::InspectorNotice*>(&element))
            information = element.HasClass("inspector-notice-information");
    };
    ForEachDescendant(host, findKind);
    EXPECT_TRUE(information) << "a slower edit is information, not a warning";

    EXPECT_TRUE(RowText(m_OtherVolume, Reason::PooledBlend).empty());
    EXPECT_TRUE(RowText(m_Volume, Reason::None).empty()) << "a row that does not pool";
    Decide(Reason::PooledBlend, m_Volume, 2);
    EXPECT_TRUE(Contains(RowText(m_Volume, Reason::PooledBlend), "2 other effects also keep the heights on the cpu"));
}

// A reason on the terrain itself costs every volume the same, so no volume shows the notice under one.
TEST_F(TerrainSlowEditNotice, ATerrainGateShowsNoSlowEditNotice)
{
    Decide(Reason::Stamp, m_Volume);
    EXPECT_FALSE(RowText(m_Volume, Reason::Stamp).empty());
    Report().TerrainRefusal = Reason::TextureLayout;
    ++Report().Revision;
    EXPECT_TRUE(RowText(m_Volume, Reason::Stamp).empty());
}

// The row follows each bake decision with no rebuild of the inspector: the user picks Average, the
// next bake refuses, and the notice appears; the user picks Set again, and it goes.
TEST_F(TerrainSlowEditNotice, TheNoticeFollowsANewBakeDecisionWithoutARebuild)
{
    Decide(Reason::None, m_Volume);
    UIElement host;
    BuildRow(host, m_Volume, Reason::PooledBlend);
    EXPECT_EQ(WarningCountIn(host), 0u);

    Decide(Reason::PooledBlend, m_Volume);
    RunRefresh();
    EXPECT_EQ(WarningCountIn(host), 1u) << "the notice waits for a rebuild";

    Decide(Reason::None, m_Volume);
    RunRefresh();
    EXPECT_EQ(WarningCountIn(host), 0u);
}
