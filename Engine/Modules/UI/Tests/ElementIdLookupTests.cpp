#include "UI/Controls/Mount.h"
#include "UI/UIElement.h"
#include "UI/UIManager.h"

#include <gtest/gtest.h>
#include <memory>

using namespace GameEngine;

namespace
{

std::unique_ptr<UIElement> MakeNamedElement(const char* id)
{
    auto element = std::make_unique<UIElement>();
    element->SetId(id);
    return element;
}

} // namespace

TEST(ElementIdLookup, TracksAttachDetachAndCrossManagerReparenting)
{
    UIManager first(nullptr);
    UIManager second(nullptr);
    first.SetRoot(MakeNamedElement("first"));
    second.SetRoot(MakeNamedElement("second"));
    auto child = MakeNamedElement("child");
    UIElement* childPointer = child.get();
    first.GetRootElement()->AddChild(std::move(child));
    EXPECT_EQ(first.GetRootElement()->FindById("child"), childPointer);
    EXPECT_EQ(second.GetRootElement()->FindById("child"), nullptr);

    child = first.GetRootElement()->TakeChild(childPointer);
    EXPECT_EQ(first.GetRootElement()->FindById("child"), nullptr);
    EXPECT_EQ(child->FindById("child"), childPointer);
    second.GetRootElement()->AddChild(std::move(child));
    EXPECT_EQ(second.GetRootElement()->FindById("child"), childPointer);
    second.GetRootElement()->RemoveChild(childPointer);
    EXPECT_EQ(second.GetRootElement()->FindById("child"), nullptr);
}

TEST(ElementIdLookup, RenamingAndClearingRemoveTheOldId)
{
    UIManager manager(nullptr);
    manager.SetRoot(MakeNamedElement("root"));
    auto child = MakeNamedElement("old");
    UIElement* childPointer = child.get();
    manager.GetRootElement()->AddChild(std::move(child));
    childPointer->SetId("new");
    EXPECT_EQ(manager.GetRootElement()->FindById("old"), nullptr);
    EXPECT_EQ(manager.GetRootElement()->FindById("new"), childPointer);
    childPointer->SetId("");
    EXPECT_EQ(manager.GetRootElement()->FindById("new"), nullptr);
}

TEST(ElementIdLookup, DuplicateIdsUseTheFirstInTreeOrderInsideTheRequestedSubtree)
{
    UIManager manager(nullptr);
    manager.SetRoot(MakeNamedElement("root"));
    auto first = MakeNamedElement("duplicate");
    auto second = MakeNamedElement("duplicate");
    UIElement* firstPointer = first.get();
    UIElement* secondPointer = second.get();
    UIElement* root = manager.GetRootElement();
    root->AddChild(std::move(second));
    root->InsertChild(0, std::move(first));
    EXPECT_EQ(root->FindById("duplicate"), firstPointer);
    EXPECT_EQ(firstPointer->FindById("duplicate"), firstPointer);
    EXPECT_EQ(secondPointer->FindById("duplicate"), secondPointer);

    secondPointer->SetId("duplicate");
    EXPECT_EQ(root->FindById("duplicate"), firstPointer);
    secondPointer->SetId("temporary");
    secondPointer->SetId("duplicate");
    EXPECT_EQ(root->FindById("duplicate"), firstPointer);
    second = root->TakeChild(secondPointer);
    root->AddChild(std::move(second));
    EXPECT_EQ(root->FindById("duplicate"), firstPointer);
    first = root->TakeChild(firstPointer);
    root->AddChild(std::move(first));
    EXPECT_EQ(root->FindById("duplicate"), secondPointer);
    root->RemoveChild(secondPointer);
    EXPECT_EQ(root->FindById("duplicate"), firstPointer);
}

TEST(ElementIdLookup, DuplicateAncestorsPrecedeDescendantsAndEarlierBranches)
{
    UIManager manager(nullptr);
    manager.SetRoot(MakeNamedElement("duplicate"));
    UIElement* root = manager.GetRootElement();
    auto branch = MakeNamedElement("branch");
    UIElement* branchPointer = branch.get();
    auto descendant = MakeNamedElement("duplicate");
    UIElement* descendantPointer = descendant.get();
    branch->AddChild(std::move(descendant));
    root->AddChild(std::move(branch));
    root->AddChild(MakeNamedElement("duplicate"));
    EXPECT_EQ(root->FindById("duplicate"), root);
    root->SetId("root");
    EXPECT_EQ(root->FindById("duplicate"), descendantPointer);
    branchPointer->SetId("duplicate");
    EXPECT_EQ(root->FindById("duplicate"), branchPointer);
    descendantPointer->SetId("temporary");
    descendantPointer->SetId("duplicate");
    EXPECT_EQ(branchPointer->FindById("duplicate"), branchPointer);
}

TEST(ElementIdLookup, DuplicateMountTargetsFollowOrdinaryChildren)
{
    auto target = MakeNamedElement("duplicate");
    UIManager manager(nullptr);
    auto mount = std::make_unique<Mount>();
    Mount* mountPointer = mount.get();
    auto child = MakeNamedElement("duplicate");
    UIElement* childPointer = child.get();
    mount->AddChild(std::move(child));
    manager.SetRoot(std::move(mount));
    mountPointer->SetTarget(target.get());
    EXPECT_EQ(mountPointer->FindById("duplicate"), childPointer);
    childPointer->SetId("temporary");
    childPointer->SetId("duplicate");
    EXPECT_EQ(mountPointer->FindById("duplicate"), childPointer);
    mountPointer->RemoveChild(childPointer);
    EXPECT_EQ(mountPointer->FindById("duplicate"), target.get());
}

TEST(ElementIdLookup, DoesNotReturnAnElementOutsideTheRequestedSubtree)
{
    UIManager manager(nullptr);
    manager.SetRoot(MakeNamedElement("root"));
    auto branch = MakeNamedElement("branch");
    UIElement* branchPointer = branch.get();
    manager.GetRootElement()->AddChild(std::move(branch));
    manager.GetRootElement()->AddChild(MakeNamedElement("sibling"));
    EXPECT_EQ(branchPointer->FindById("sibling"), nullptr);
    EXPECT_EQ(branchPointer->FindById("root"), nullptr);
}

TEST(ElementIdLookup, MountRetargetingAndActivationAreImmediatelyVisible)
{
    auto first = MakeNamedElement("first-panel");
    first->AddChild(MakeNamedElement("first-content"));
    auto second = MakeNamedElement("second-panel");
    second->AddChild(MakeNamedElement("second-content"));
    UIManager manager(nullptr);
    auto root = MakeNamedElement("root");
    auto mount = std::make_unique<Mount>();
    Mount* mountPointer = mount.get();
    mount->SetTarget(first.get());
    root->AddChild(std::move(mount));
    manager.SetRoot(std::move(root));
    EXPECT_NE(manager.GetRootElement()->FindById("first-content"), nullptr);

    mountPointer->SetTarget(second.get());
    EXPECT_EQ(manager.GetRootElement()->FindById("first-content"), nullptr);
    EXPECT_NE(manager.GetRootElement()->FindById("second-content"), nullptr);
    mountPointer->SwapTargetForActivation(first.get());
    EXPECT_NE(manager.GetRootElement()->FindById("first-content"), nullptr);
    EXPECT_EQ(manager.GetRootElement()->FindById("second-content"), nullptr);
    mountPointer->SetTarget(nullptr);
    EXPECT_EQ(manager.GetRootElement()->FindById("first-content"), nullptr);
}

TEST(ElementIdLookup, DestroyedMountTargetsAndReplacedRootsLeaveNoResults)
{
    auto target = MakeNamedElement("target");
    UIManager manager(nullptr);
    auto mount = std::make_unique<Mount>();
    mount->SetTarget(target.get());
    manager.SetRoot(std::move(mount));
    EXPECT_EQ(manager.GetRootElement()->FindById("target"), target.get());
    target.reset();
    EXPECT_EQ(manager.GetRootElement()->FindById("target"), nullptr);
    manager.SetRoot(MakeNamedElement("replacement"));
    EXPECT_EQ(manager.GetRootElement()->FindById("target"), nullptr);
    EXPECT_EQ(manager.GetRootElement()->FindById("replacement"), manager.GetRootElement());
}
