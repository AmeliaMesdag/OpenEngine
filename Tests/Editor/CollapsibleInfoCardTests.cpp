// The editor's explanatory card, as everything that reads a UI tree sees it.
//
// The card is a control that OWNS the Label carrying its copy, so a card's text is not a child of
// whatever added it. That makes one property load-bearing for every consumer that is not the
// renderer — search, a tree dump, a notice test: the copy has to be in the tree from the frame the
// card is built, not from the frame it is first laid out. Chrome that appears only after a layout
// pass reads as an empty box to all of them, and reads as "the notice was never added" to a test.
//
// Folding stays a layout decision (OnPostLayout measures the laid-out copy against one row), which
// is why the chevron and the action are asserted here only as present-and-quiet.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "InspectorLayoutFixture.h"
#include "UI/Controls/CollapsibleInfoCard.h"
#include "UI/Controls/Label.h"
#include "UI/Parsers/XMLParser.h"
#include "UI/ResolvedStyle.h"
#include "UI/UIElement.h"
#include "UI/UIStyle.h"

using GameEngine::Label;
using GameEngine::UIElement;
using GameEngine::EditorUI::CollapsibleInfoCard;
using InspectorLayoutTesting::FindByClass;
using InspectorLayoutTesting::InspectorLayoutFixture;

namespace
{

std::vector<std::string> AllLabelTextIn(const UIElement& parent)
{
    std::vector<std::string> texts;
    for (const auto& child : parent.GetChildren())
    {
        if (!child)
            continue;
        if (const auto* label = dynamic_cast<const Label*>(child.get()))
            texts.push_back(label->GetText());
        for (std::string& nested : AllLabelTextIn(*child))
            texts.push_back(std::move(nested));
    }
    return texts;
}

bool HoldsText(const UIElement& parent, const std::string& text)
{
    for (const std::string& candidate : AllLabelTextIn(parent))
        if (candidate == text)
            return true;
    return false;
}

} // namespace

// The property the notice tests rest on: no UIManager, no layout pass, and the copy is still
// there to be read.
TEST(CollapsibleInfoCard, CarriesItsCopyAsALabelFromConstruction)
{
    const std::string copy = "Region is every terrain - no edge, no falloff.";
    CollapsibleInfoCard card(copy);

    EXPECT_EQ(card.GetFullText(), copy);
    EXPECT_TRUE(HoldsText(card, copy)) << "the card's copy is not in the element tree";
}

// A card added to a host is readable through the host, which is how every notice site reaches it.
TEST(CollapsibleInfoCard, ItsCopyIsReachableThroughTheHostThatAddedIt)
{
    UIElement host;
    const std::string copy = "Add Rule below, then give the rule a condition.";
    host.AddChild(std::make_unique<CollapsibleInfoCard>(copy));

    EXPECT_TRUE(HoldsText(host, copy));
}

// Replacing the copy replaces what the tree carries, so a card that is rebuilt in place cannot
// leave the previous material's sentence behind it.
TEST(CollapsibleInfoCard, SetTextReplacesTheCopyInTheTree)
{
    CollapsibleInfoCard card("The first thing it said.");
    card.SetText("The second thing it said.");

    // The new copy first: it is what makes the absence below mean "replaced" rather than "the
    // tree is empty", which an empty tree would satisfy on its own.
    ASSERT_TRUE(HoldsText(card, "The second thing it said."));
    EXPECT_FALSE(HoldsText(card, "The first thing it said."));
}

// A layout file declares a card by its tag, and the text attribute is the copy.
TEST(CollapsibleInfoCard, ALayoutDeclaresACardByItsTagAndText)
{
    std::unique_ptr<UIElement> root;
    ASSERT_TRUE(GameEngine::UIParsing::XMLParser::ParseLayoutFromString(
        R"(<CollapsibleInfoCard text="Bake &amp; save. Then reload." />)", root));

    const auto* card = dynamic_cast<const CollapsibleInfoCard*>(root.get());
    ASSERT_NE(card, nullptr) << "the layout tag does not make a CollapsibleInfoCard";
    EXPECT_EQ(card->GetFullText(), "Bake & save. Then reload.");
}

// Cards read expanded, and one that was never laid out was never measured — so it has made no
// fold decision and offers no action to undo one.
TEST(CollapsibleInfoCard, AnUnmeasuredCardIsExpandedAndNotFoldable)
{
    CollapsibleInfoCard card("A sentence. And a second one, which the preview would cut.");

    EXPECT_TRUE(card.IsExpanded());
    EXPECT_FALSE(card.IsFoldable());
    // Expanded means the whole text, not the preview the first sentence would give.
    EXPECT_TRUE(HoldsText(card, "A sentence. And a second one, which the preview would cut."));
}

// A card whose copy changes while it is on screen (a live stats line) keeps its chevron until a
// measurement of the new copy says otherwise: dropping it on every change would hide the chevron
// for a frame, and with it the width and the wrapping it takes.
TEST(CollapsibleInfoCardFold, NewCopyKeepsTheChevronUntilItIsMeasured)
{
    const std::string twoRows =
        "Playing - Particles 12 - Preview 3.5 s. Simulation 0.4 ms - Ticks 210 - Catch-up dropped 0.0 s, "
        "so the line is long enough to wrap onto a second row at this width.";
    const std::string otherTwoRows =
        "Playing - Particles 18 - Preview 7.7 s. Simulation 0.6 ms - Ticks 462 - Catch-up dropped 0.0 s, "
        "so the line is long enough to wrap onto a second row at this width.";
    auto root = std::make_unique<UIElement>();
    auto card = std::make_unique<CollapsibleInfoCard>(twoRows);
    CollapsibleInfoCard* cardRaw = card.get();
    root->AddChild(std::move(card));

    InspectorLayoutFixture fixture;
    fixture.ViewportW = 360;
    fixture.ExtraSheets = {"Assets/UI/theme/core.css", "Assets/UI/controls/CollapsibleInfoCard/CollapsibleInfoCard.css"};
    if (!fixture.Build(std::move(root)))
        GTEST_SKIP() << fixture.Diagnostic;
    UIElement* chevron = FindByClass(cardRaw, "editor-info-card-chevron");
    ASSERT_NE(chevron, nullptr);
    ASSERT_TRUE(cardRaw->IsFoldable()) << "the long copy did not wrap, so the fixture cannot show a fold";

    cardRaw->SetText(otherTwoRows);
    EXPECT_TRUE(cardRaw->IsFoldable()) << "new copy dropped the fold state before it was measured";
    EXPECT_FALSE(chevron->HasClass("blank")) << "new copy blanked the chevron before it was measured";

    for (int frame = 0; frame < 3; ++frame)
        fixture.Ui->Update(1.0f / 60.0f, /*interactive=*/true);
    EXPECT_TRUE(cardRaw->IsFoldable());

    cardRaw->SetText("Short.");
    for (int frame = 0; frame < 3; ++frame)
        fixture.Ui->Update(1.0f / 60.0f, /*interactive=*/true);
    EXPECT_FALSE(cardRaw->IsFoldable()) << "the measurement of one row still ends the fold";
    EXPECT_TRUE(chevron->HasClass("blank"));
}

// A card that fits one row shows no chevron glyph but keeps the chevron's box, so its copy starts
// at the same x as the copy of a card that wraps and folds.
TEST(CollapsibleInfoCardFold, OneRowAndWrappedCardsStartTheirCopyAtOneX)
{
    auto root = std::make_unique<UIElement>();
    auto oneRow = std::make_unique<CollapsibleInfoCard>("Short.");
    auto wrapped = std::make_unique<CollapsibleInfoCard>(
        "A long explanation that has to wrap onto a second row at this width, so the card folds and "
        "shows its chevron beside the copy.");
    CollapsibleInfoCard* oneRowRaw = oneRow.get();
    CollapsibleInfoCard* wrappedRaw = wrapped.get();
    root->AddChild(std::move(oneRow));
    root->AddChild(std::move(wrapped));

    InspectorLayoutFixture fixture;
    fixture.ViewportW = 360;
    fixture.ExtraSheets = {"Assets/UI/theme/core.css", "Assets/UI/controls/CollapsibleInfoCard/CollapsibleInfoCard.css"};
    if (!fixture.Build(std::move(root)))
        GTEST_SKIP() << fixture.Diagnostic;
    ASSERT_FALSE(oneRowRaw->IsFoldable()) << "the short copy wrapped, so the fixture cannot show a one-row card";
    ASSERT_TRUE(wrappedRaw->IsFoldable()) << "the long copy did not wrap, so the fixture cannot show a fold";

    const UIElement* oneRowCopy = FindByClass(oneRowRaw, "editor-info-card-copy");
    const UIElement* wrappedCopy = FindByClass(wrappedRaw, "editor-info-card-copy");
    ASSERT_NE(oneRowCopy, nullptr);
    ASSERT_NE(wrappedCopy, nullptr);
    EXPECT_FLOAT_EQ(oneRowCopy->GetLayoutX(), wrappedCopy->GetLayoutX())
        << "the one-row card's copy does not start where the wrapped card's copy starts";
}

// A folded card cuts its one line with an ellipsis, never mid-word with no mark, and its "show more"
// starts at least 24 px after the cut copy, wider than a word space, so the two do not read as one line.
TEST(CollapsibleInfoCardFold, AFoldedLineEndsInAnEllipsisClearOfShowMore)
{
    constexpr float kMinShowMoreGap = 24.0f;
    auto root = std::make_unique<UIElement>();
    auto card = std::make_unique<CollapsibleInfoCard>(
        "A first sentence long enough that a folded card cannot hold it on one line at this width, so the "
        "line is cut before the action. A second sentence.");
    CollapsibleInfoCard* cardRaw = card.get();
    root->AddChild(std::move(card));

    InspectorLayoutFixture fixture;
    fixture.ViewportW = 360;
    fixture.ExtraSheets = {"Assets/UI/theme/core.css", "Assets/UI/controls/CollapsibleInfoCard/CollapsibleInfoCard.css"};
    if (!fixture.Build(std::move(root)))
        GTEST_SKIP() << fixture.Diagnostic;
    ASSERT_TRUE(cardRaw->IsFoldable()) << "the long copy did not wrap, so the fixture cannot show a fold";
    cardRaw->SetExpanded(false);
    for (int frame = 0; frame < 3; ++frame)
        fixture.Ui->Update(1.0f / 60.0f, /*interactive=*/true);

    const UIElement* copy = FindByClass(cardRaw, "editor-info-card-copy");
    const UIElement* showMore = FindByClass(cardRaw, "editor-info-card-toggle");
    ASSERT_NE(copy, nullptr);
    ASSERT_NE(showMore, nullptr);
    ASSERT_TRUE(copy->HasClass("clamped"));
    ASSERT_FALSE(showMore->HasClass("hidden"));
    const GameEngine::ResolvedStyle& copyStyle = copy->GetResolvedStyle();
    EXPECT_EQ(copyStyle.Visual.WhiteSpace, GameEngine::WhiteSpace::NoWrap);
    EXPECT_EQ(copyStyle.Layout.Overflow, GameEngine::Overflow::Hidden);
    EXPECT_EQ(copyStyle.Visual.TextOverflow, GameEngine::TextOverflowMode::Ellipsis)
        << "the folded line is cut with no ellipsis";

    const float copyRight = copy->GetLayoutX() + copy->GetLayoutWidth();
    const float showMoreTextLeft = showMore->GetLayoutX() + showMore->GetResolvedStyle().Layout.Padding.Left;
    EXPECT_GE(showMoreTextLeft - copyRight, kMinShowMoreGap) << "\"show more\" sits too close to the cut copy";
}
