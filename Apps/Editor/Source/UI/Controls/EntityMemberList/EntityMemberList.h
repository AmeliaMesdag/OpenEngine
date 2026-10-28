#pragma once

#include "ECS/Entity.h"
#include "Types/Types.h"
#include "UI/UIElement.h"

#include <functional>
#include <string>
#include <vector>

namespace GameEngine::EditorUI
{

/// The members of one owner, then the candidates it offers. A row has a member checkbox, a glyph, a
/// title, a note and a mode. The caller owns the data and the undo steps: each gesture calls one handler,
/// and the caller builds the list again. Layouts: EntityMemberList.uxml, EntityMemberRow.uxml.
class EntityMemberList final : public UIElement
{
  public:
    /// One mode that a row offers. A row offers one mode, shown as a label, or two, shown as
    /// segments. A `Warning` mode takes something away: its filled segment shows the warning color.
    struct Mode
    {
        uint8 Value = 0;
        std::string Label;
        std::string Tooltip;
        bool Warning = false;
    };

    /// One entity of the list. `GlyphClasses` draw the glyph (no glyph when empty). `CurrentMode` is a
    /// member's mode; a candidate ignores it. A row that is not `Live` is a deleted member: it shows
    /// `Words::DeletedTitle`, says that undo restores it, and cannot be selected.
    struct Row
    {
        ECS::EntityHandle Entity{};
        std::string Title;
        std::vector<std::string> GlyphClasses;
        std::string GlyphTooltip;
        std::string Note;
        std::vector<Mode> Modes;
        uint8 CurrentMode = 0;
        bool Live = true;
    };

    /// The owner's words. `MemberTooltip` and `CandidateTooltip` are the checkbox tooltips.
    struct Words
    {
        std::string Heading;
        std::string HeadingTooltip;
        std::string Empty;
        std::string DeletedTitle;
        std::string MemberTooltip;
        std::string CandidateTooltip;
    };

    /// The rows in their order: the members first, then the candidates. A non-empty `AddRefusal`
    /// turns off every candidate's checkbox and is its tooltip (the owner holds no more members).
    /// While not `Editable`, no checkbox and no mode can change.
    struct Content
    {
        Words Text;
        std::vector<Row> Members;
        std::vector<Row> Candidates;
        std::string AddRefusal;
        bool Editable = true;
    };

    /// Called with the row's entity. `Select` is called on a click on a live row's title.
    struct Handlers
    {
        std::function<void(ECS::EntityHandle)> Add;
        std::function<void(ECS::EntityHandle)> Remove;
        std::function<void(ECS::EntityHandle, uint8)> SetMode;
        std::function<void(ECS::EntityHandle)> Select;
    };

    EntityMemberList(const Content& content, const Handlers& handlers);
};

} // namespace GameEngine::EditorUI
