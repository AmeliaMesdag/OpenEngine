#include "UI/Controls/EntityMemberList/EntityMemberList.h"

#include "Inspectors/InspectorDragHelpers.h"
#include "Logger/Logger.h"
#include "UI/ControlLayoutLoader.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/Label.h"
#include "UI/Controls/Toggle.h"
#include "UI/UIEvents.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace GameEngine::EditorUI
{
namespace
{

using Content = EntityMemberList::Content;
using Handlers = EntityMemberList::Handlers;
using Mode = EntityMemberList::Mode;
using Row = EntityMemberList::Row;

constexpr const char* kLayoutAssetPath = "UI/controls/EntityMemberList/EntityMemberList.uxml";
constexpr const char* kRowLayoutAssetPath = "UI/controls/EntityMemberList/EntityMemberRow.uxml";
constexpr const char* kStyleAssetPath = "UI/controls/EntityMemberList/EntityMemberList.css";

// core.css: display none.
constexpr const char* kHiddenClass = "hidden";
constexpr const char* kDeletedNote = "undo restores it";
// The row layout holds two segments: a row offers one mode or two.
constexpr std::size_t kMaxModes = 2;
// Button.css: the filled segment of a segmented row.
constexpr const char* kSelectedSegmentClass = "segmented-button-selected";

// The parts of one row layout.
struct RowParts
{
    Toggle* Member = nullptr;
    UIElement* Glyph = nullptr;
    Label* Title = nullptr;
    Label* Note = nullptr;
    UIElement* Modes = nullptr;
    std::array<Button*, kMaxModes> Segments{};
    Label* ModeLabel = nullptr;

    bool Complete() const
    {
        return Member && Glyph && Title && Note && Modes && Segments[0] && Segments[1] && ModeLabel;
    }
};

RowParts FindRowParts(UIElement& row)
{
    RowParts parts;
    parts.Member = dynamic_cast<Toggle*>(InspectorDrag::FindChildByClass(&row, "entity-member-toggle"));
    parts.Glyph = InspectorDrag::FindChildByClass(&row, "entity-member-glyph");
    UIElement* text = InspectorDrag::FindChildByClass(&row, "entity-member-text");
    parts.Title = dynamic_cast<Label*>(InspectorDrag::FindChildByClass(text, "entity-member-title"));
    parts.Note = dynamic_cast<Label*>(InspectorDrag::FindChildByClass(text, "entity-member-note"));
    parts.Modes = InspectorDrag::FindChildByClass(&row, "entity-member-mode");
    if (!parts.Modes)
        return parts;
    std::size_t segment = 0;
    for (const auto& child : parts.Modes->GetChildren())
    {
        if (auto* button = dynamic_cast<Button*>(child.get()); button && segment < kMaxModes)
            parts.Segments[segment++] = button;
    }
    parts.ModeLabel = dynamic_cast<Label*>(InspectorDrag::FindChildByClass(parts.Modes, "entity-member-mode-label"));
    return parts;
}

// A segment of a member's modes, filled when it is the member's mode. A click on another segment
// calls SetMode. The button attaches Button.css to itself; this sheet goes after it, so that the
// control's single-class rules outrank the segmented look at equal specificity.
void ShowModeSegment(Button& segment, const Row& row, const Mode& mode, bool member, bool editable,
                     const Handlers& handlers)
{
    segment.RequestSubtreeStyleAssetPath(kStyleAssetPath, "editor");
    segment.SetText(mode.Label);
    segment.SetTooltip(mode.Tooltip);
    const bool current = member && row.CurrentMode == mode.Value;
    if (current)
        segment.AddClass(kSelectedSegmentClass);
    if (current && mode.Warning)
        segment.AddClass("entity-member-segment-warning");
    segment.SetEnabled(member && editable);
    if (current)
        return;
    segment.RegisterEventHandler(kEventButtonClick,
                                 [setMode = handlers.SetMode, entity = row.Entity, value = mode.Value](UIEvent&) {
                                     if (setMode)
                                         setMode(entity, value);
                                 });
}

// The row's modes: one mode as a label, two as segments that only a member of an editable list can
// switch.
void ShowModes(const RowParts& parts, const Row& row, bool member, bool editable, const Handlers& handlers)
{
    if (row.Modes.empty() || row.Modes.size() > kMaxModes)
    {
        Logger::Log::Error("EntityMemberList: a row offers one mode or two, and this row offers {}.", row.Modes.size());
        parts.Modes->AddClass(kHiddenClass);
        return;
    }
    if (row.Modes.size() == 1)
    {
        const Mode& mode = row.Modes.front();
        parts.Modes->AddClass("entity-member-mode-single");
        parts.ModeLabel->SetText(mode.Label);
        parts.ModeLabel->SetTooltip(mode.Tooltip);
        for (Button* segment : parts.Segments)
            segment->AddClass(kHiddenClass);
        return;
    }
    parts.ModeLabel->AddClass(kHiddenClass);
    for (std::size_t i = 0; i < kMaxModes; ++i)
        ShowModeSegment(*parts.Segments[i], row, row.Modes[i], member, editable, handlers);
}

void ShowMemberToggle(Toggle& toggle, const Row& row, bool member, const Content& content, const Handlers& handlers)
{
    toggle.SetValueWithoutNotify(member);
    toggle.SetTooltip(member ? content.Text.MemberTooltip : content.Text.CandidateTooltip);
    const bool refused = !member && !content.AddRefusal.empty();
    if (refused)
        toggle.SetTooltip(content.AddRefusal);
    if (refused || !content.Editable)
        toggle.SetEnabled(false);
    toggle.SetOnValueChanged(
        [add = handlers.Add, remove = handlers.Remove, entity = row.Entity, member](const bool& checked) {
            const std::function<void(ECS::EntityHandle)>& handler = checked ? add : remove;
            if (checked != member && handler)
                handler(entity);
        });
}

void ShowGlyph(UIElement& glyph, const Row& row)
{
    if (row.GlyphClasses.empty())
    {
        glyph.AddClass(kHiddenClass);
        return;
    }
    for (const std::string& glyphClass : row.GlyphClasses)
        glyph.AddClass(glyphClass);
    glyph.SetTooltip(row.GlyphTooltip);
}

void ShowTitle(Label& title, const Row& row, bool member, const Content& content, const Handlers& handlers)
{
    if (!row.Live)
    {
        title.AddClass("entity-member-title-deleted");
        title.SetText(content.Text.DeletedTitle);
        return;
    }
    if (!member)
        title.AddClass("entity-member-title-out");
    title.SetText(row.Title);
    title.SetTooltip("Click to select it");
    title.RegisterEventHandler(kEventMouseUp, [select = handlers.Select, entity = row.Entity](UIEvent& event) {
        if (event.Button == 0 && select)
            select(entity);
    });
}

void ShowNote(Label& note, const Row& row)
{
    const std::string text = row.Live ? row.Note : std::string(kDeletedNote);
    if (text.empty())
        note.AddClass(kHiddenClass);
    else
        note.SetText(text);
}

// The row of one entity, or null with an error when the row layout does not load or lacks a part.
std::unique_ptr<UIElement> BuildRow(const Row& row, bool member, const Content& content, const Handlers& handlers)
{
    std::unique_ptr<UIElement> element = Editor::ControlLayoutLoader::Load(kRowLayoutAssetPath, {});
    if (!element)
        return nullptr;
    const RowParts parts = FindRowParts(*element);
    if (!parts.Complete())
    {
        Logger::Log::Error("EntityMemberList: {} needs the member checkbox, the glyph, the title, the note, two "
                           "mode segments and the mode label.",
                           kRowLayoutAssetPath);
        return nullptr;
    }
    element->AddClass(member ? "entity-member-in" : "entity-member-out");
    ShowMemberToggle(*parts.Member, row, member, content, handlers);
    ShowGlyph(*parts.Glyph, row);
    ShowTitle(*parts.Title, row, member, content, handlers);
    ShowNote(*parts.Note, row);
    ShowModes(parts, row, member, content.Editable, handlers);
    return element;
}

} // namespace

EntityMemberList::EntityMemberList(const Content& content, const Handlers& handlers)
{
    AddClass("entity-member-group");
    RequestSubtreeStyleAssetPath(kStyleAssetPath, "editor");

    std::unique_ptr<UIElement> header = Editor::ControlLayoutLoader::Load(kLayoutAssetPath, {});
    if (!header)
        return;
    auto* heading = dynamic_cast<Label*>(InspectorDrag::FindChildByClass(header.get(), "entity-member-heading"));
    auto* empty = dynamic_cast<Label*>(InspectorDrag::FindChildByClass(header.get(), "entity-member-empty"));
    if (!heading || !empty)
    {
        Logger::Log::Error("EntityMemberList: {} needs the heading and the empty text.", kLayoutAssetPath);
        return;
    }
    heading->SetText(content.Text.Heading);
    heading->SetTooltip(content.Text.HeadingTooltip);
    empty->SetText(content.Text.Empty);
    if (!content.Members.empty() || !content.Candidates.empty())
        empty->AddClass(kHiddenClass);
    AddChild(std::move(header));
    for (const Row& row : content.Members)
    {
        if (std::unique_ptr<UIElement> element = BuildRow(row, true, content, handlers))
            AddChild(std::move(element));
    }
    for (const Row& row : content.Candidates)
    {
        if (std::unique_ptr<UIElement> element = BuildRow(row, false, content, handlers))
            AddChild(std::move(element));
    }
}

} // namespace GameEngine::EditorUI
