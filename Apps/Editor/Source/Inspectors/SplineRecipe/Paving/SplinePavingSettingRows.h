#pragma once

namespace GameEngine
{

struct InspectorContext;

namespace Components
{
struct SplinePaving;
}

/// The paving's either-or settings as picture tiles (EditorUI::PictureChoiceField), each into
/// `context.Parent`: what the curb builds at a corner, how the paving meets a road, and what the
/// paving lies on. `shown` is the paving the rows show. A pick is one undo step over the selection.
void AddPavingCornerRow(const InspectorContext& context, const Components::SplinePaving& shown);
void AddPavingMouthRow(const InspectorContext& context, const Components::SplinePaving& shown);
void AddPavingConformRow(const InspectorContext& context, const Components::SplinePaving& shown);

/// Ground Authority as a dropdown (Auto with what it resolves to, Owns, Grades, None) into
/// `context.Parent`, with the note on what Owns does under it. A pick is one undo step over the
/// selection.
void AddPavingGroundAuthorityRow(const InspectorContext& context, const Components::SplinePaving& shown);

/// The curb's width and height as sliders with their number boxes over the fields' ranges (0 to 10 m
/// and 0.01 to 10 m), into `context.Parent`. A drag is one undo step.
void AddPavingBorderSizeRows(const InspectorContext& context, const Components::SplinePaving& shown);

/// The mouth radius as a slider with its number box over the field's range, into `context.Parent`.
/// The row reads inactive with a Straight mouth, which has no curve to size. A drag is one undo step.
void AddPavingMouthRadiusRow(const InspectorContext& context, const Components::SplinePaving& shown);

} // namespace GameEngine
