#include "Inspectors/SplineRecipe/Paving/SplinePavingSettingRows.h"

#include "InspectorRegistry.h"
#include "Inspectors/DefaultComponentInspector.h"
#include "Inspectors/InspectorComponent/InspectorComponentRowHelpers.h"
#include "Inspectors/InspectorUIHelpers.h"
#include "UI/Controls/EnumField.h"
#include "UI/Controls/PictureChoiceField.h"

#include "Components/Spline/SplinePaving.h"
#include "ECS/Component/ComponentFieldRegistry.h"
#include "ECS/ECSTemplates.h"
#include "ECS/World.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace GameEngine
{
namespace
{

using Components::SplinePaving;

template <typename TEnum>
struct PavingChoice
{
    TEnum Value;
    const char* Label;
    const char* Description;
    const char* PictureClass;
};

// The tiles in order: a tile's index is its index here.
constexpr PavingChoice<Components::SplineWallCorner> kCornerChoices[] = {
    {Components::SplineWallCorner::Miter, "Miter", "A sharp corner.", "option-picture--paving-corner-miter"},
    {Components::SplineWallCorner::Round, "Round", "A rounded corner.", "option-picture--paving-corner-round"},
};

constexpr PavingChoice<Components::SplinePavingMouth> kMouthChoices[] = {
    {Components::SplinePavingMouth::Flared, "Flared", "Curves into the road.", "option-picture--paving-mouth-flared"},
    {Components::SplinePavingMouth::Straight, "Straight", "A square corner at the road.",
     "option-picture--paving-mouth-straight"},
};

constexpr PavingChoice<Components::SplineConformTarget> kConformChoices[] = {
    {Components::SplineConformTarget::TerrainOnly, "Terrain Only", "Lies on the terrain only.",
     "option-picture--paving-conform-terrain-only"},
    {Components::SplineConformTarget::Scene, "Scene", "Lies on decks and props.",
     "option-picture--paving-conform-scene"},
};

// The dropdown names what Auto resolves to (Components::ResolveGroundAuthority), as the terrain
// inspector's Auto rows do.
constexpr EnumEntry<Components::SplineGroundAuthority> kGroundAuthorities[] = {
    {Components::SplineGroundAuthority::Auto, "Auto (Owns)"},
    {Components::SplineGroundAuthority::Owns, "Owns"},
    {Components::SplineGroundAuthority::Grades, "Grades"},
    {Components::SplineGroundAuthority::None, "None"},
};
constexpr const char* kGroundAuthorityField = "GroundAuthority";
constexpr const char* kOwnsNote = "Grades the ground to a smooth surface that follows the rim and meets each road at its "
                                  "height. Clears its grass, paints it Dirt, and claims it, so other grading stops at its "
                                  "edge. Planting beds cut out of the paving are graded and painted too.";

constexpr const char* kUndoPrefix = "Change Spline Paving ";
// The curb's width and height reach 1 m on the slider; a typed value takes the field's whole range.
constexpr float kCurbSliderMaxMeters = 1.0f;
constexpr const char* kMouthRadiusStraightReason = "A Straight mouth has no curve. Choose Flared to set its radius.";

template <typename TEnum>
size_t ChoiceIndex(std::span<const PavingChoice<TEnum>> choices, TEnum value)
{
    for (size_t i = 0; i < choices.size(); ++i)
    {
        if (choices[i].Value == value)
            return i;
    }
    return 0;
}

// Writes the choice at `index` into the field `member` of every selected paving, one undo step,
// and rebuilds the inspector so the rows that depend on the field follow it.
template <typename TEnum>
void CommitChoice(const InspectorContext& context, std::span<const PavingChoice<TEnum>> choices,
                  TEnum SplinePaving::*member, const std::string& undoName, size_t index)
{
    if (!context.World || index >= choices.size())
        return;
    const TEnum value = choices[index].Value;
    InspectorDrag::CommitComponentsWithUndo<SplinePaving>(
        context.World, context.Entity, InspectorDrag::GetAdditionalEntities(context), context.ChangeNotifications,
        context.Undo, undoName, [member, value](SplinePaving& paving) { paving.*member = value; });
    if (context.RequestInspectorRefresh)
        context.RequestInspectorRefresh();
}

// The row label of the reflected field `fieldName`, as the reflected rows show it.
std::string FieldLabel(const ECS::FieldInfo* field, std::string_view fieldName)
{
    return field ? FieldDisplayName(*field) : std::string(fieldName);
}

// The row of the reflected field `fieldName`: its label and tooltip from the reflection, the tiles of
// `choices`, and the override mark of a single selection.
template <typename TEnum>
void AddChoiceRow(const InspectorContext& context, std::string_view fieldName,
                  std::span<const PavingChoice<TEnum>> choices, TEnum SplinePaving::*member,
                  const SplinePaving& shown)
{
    if (!context.Parent || !context.World)
        return;
    const ECS::ComponentTypeId typeId = ECS::GetComponentTypeId<SplinePaving>();
    const ECS::FieldInfo* field = ECS::ComponentFieldRegistry::FindField(typeId, fieldName);
    const std::string label = FieldLabel(field, fieldName);
    std::vector<EditorUI::PictureChoiceField::Choice> tiles;
    for (const PavingChoice<TEnum>& choice : choices)
        tiles.push_back({choice.Label, choice.Description, choice.PictureClass});
    auto picker = std::make_unique<EditorUI::PictureChoiceField>(std::move(tiles), ChoiceIndex(choices, shown.*member));
    const std::string undoName = kUndoPrefix + label;
    picker->SetOnSelected([context, choices, member, undoName](size_t index)
                          { CommitChoice(context, choices, member, undoName, index); });
    auto* added = InspectorUI::AddPictureChoiceRow(context.Parent, label, std::move(picker),
                                                   InspectorDrag::ReflectedTooltip(typeId, fieldName));
    InspectorDrag::MarkReflectedFieldOverride(context, typeId, InspectorUI::RowOfControl(added), fieldName);
}

} // namespace

void AddPavingCornerRow(const InspectorContext& context, const SplinePaving& shown)
{
    AddChoiceRow<Components::SplineWallCorner>(context, "BorderCorner", kCornerChoices, &SplinePaving::BorderCorner,
                                               shown);
}

void AddPavingMouthRow(const InspectorContext& context, const SplinePaving& shown)
{
    AddChoiceRow<Components::SplinePavingMouth>(context, "Mouth", kMouthChoices, &SplinePaving::Mouth, shown);
}

void AddPavingConformRow(const InspectorContext& context, const SplinePaving& shown)
{
    AddChoiceRow<Components::SplineConformTarget>(context, "ConformTarget", kConformChoices,
                                                  &SplinePaving::ConformTarget, shown);
}

void AddPavingGroundAuthorityRow(const InspectorContext& context, const SplinePaving& shown)
{
    if (!context.Parent || !context.World)
        return;
    const ECS::ComponentTypeId typeId = ECS::GetComponentTypeId<SplinePaving>();
    const std::string label = FieldLabel(ECS::ComponentFieldRegistry::FindField(typeId, kGroundAuthorityField),
                                         kGroundAuthorityField);
    auto* field = InspectorUI::AddEnumRow(context.Parent, label, kGroundAuthorities, shown.GroundAuthority,
                                          InspectorDrag::ReflectedTooltip(typeId, kGroundAuthorityField));
    const std::string undoName = kUndoPrefix + label;
    field->SetOnValueChanged(
        [context, undoName](Components::SplineGroundAuthority value)
        {
            InspectorDrag::CommitComponentsWithUndo<SplinePaving>(
                context.World, context.Entity, InspectorDrag::GetAdditionalEntities(context),
                context.ChangeNotifications, context.Undo, undoName,
                [value](SplinePaving& paving) { paving.GroundAuthority = value; });
            if (context.RequestInspectorRefresh)
                context.RequestInspectorRefresh();
        });
    InspectorDrag::MarkReflectedFieldOverride(context, typeId, InspectorUI::RowOfControl(field),
                                              kGroundAuthorityField);
    if (Components::ResolveGroundAuthority(shown) == Components::SplineGroundAuthority::Owns)
        InspectorUI::AddInfoCard(context.Parent, kOwnsNote);
}

void AddPavingBorderSizeRows(const InspectorContext& context, const SplinePaving& shown)
{
    InspectorDrag::AddReflectedLengthSliderRow(context, "BorderWidth", &SplinePaving::BorderWidth, shown, kUndoPrefix,
                                               kCurbSliderMaxMeters);
    InspectorDrag::AddReflectedLengthSliderRow(context, "BorderHeight", &SplinePaving::BorderHeight, shown,
                                               kUndoPrefix, kCurbSliderMaxMeters);
}

void AddPavingMouthRadiusRow(const InspectorContext& context, const SplinePaving& shown)
{
    UIElement* row =
        InspectorDrag::AddReflectedLengthSliderRow(context, "MouthRadius", &SplinePaving::MouthRadius, shown,
                                                   kUndoPrefix);
    if (row && shown.Mouth == Components::SplinePavingMouth::Straight)
        InspectorUI::DisableRow(row, kMouthRadiusStraightReason);
}

} // namespace GameEngine
