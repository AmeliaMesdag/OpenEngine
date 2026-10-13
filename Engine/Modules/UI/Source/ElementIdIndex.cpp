#include "UI/Internal/ElementIdIndex.h"

#include "UI/UIElement.h"

#include <algorithm>

namespace GameEngine::UI
{
namespace
{

size_t LookupDepth(const UIElement& element)
{
    size_t depth = 0;
    for (const UIElement* parent = element.GetDfsParent(); parent; parent = parent->GetDfsParent())
        ++depth;
    return depth;
}

bool PrecedesInLookupOrder(const UIElement& first, const UIElement& second)
{
    const size_t firstDepth = LookupDepth(first);
    const size_t secondDepth = LookupDepth(second);
    const UIElement* firstBranch = &first;
    const UIElement* secondBranch = &second;
    for (size_t depth = firstDepth; depth > secondDepth; --depth)
        firstBranch = firstBranch->GetDfsParent();
    for (size_t depth = secondDepth; depth > firstDepth; --depth)
        secondBranch = secondBranch->GetDfsParent();
    if (firstBranch == secondBranch)
        return firstDepth < secondDepth;

    // Both matches are reachable from the requested subtree, so their chains
    // share an ancestor. Compare the branches at their first divergence.
    while (firstBranch->GetDfsParent() != secondBranch->GetDfsParent())
    {
        firstBranch = firstBranch->GetDfsParent();
        secondBranch = secondBranch->GetDfsParent();
    }
    for (const auto& child : firstBranch->GetDfsParent()->GetChildren())
    {
        if (child.get() == firstBranch)
            return true;
        if (child.get() == secondBranch)
            return false;
    }
    // A Mount target follows every ordinary child in lookup order.
    return false;
}

} // namespace

void ElementIdIndex::Register(UIElement& element)
{
    if (element.GetId().empty())
        return;
    auto& matches = m_Elements[element.GetIdHash()];
    if (std::find(matches.begin(), matches.end(), &element) == matches.end())
        matches.push_back(&element);
}

void ElementIdIndex::Unregister(UIElement& element)
{
    if (element.GetId().empty())
        return;
    auto entry = m_Elements.find(element.GetIdHash());
    if (entry == m_Elements.end())
        return;
    std::erase(entry->second, &element);
    if (entry->second.empty())
        m_Elements.erase(entry);
}

UIElement* ElementIdIndex::Find(const UIElement& subtree, std::string_view id) const
{
    const auto entry = m_Elements.find(HashStringId(id));
    if (entry == m_Elements.end())
        return nullptr;
    UIElement* firstMatch = nullptr;
    for (UIElement* element : entry->second)
    {
        // StringId collisions must not alias unrelated authored ids.
        if (element->GetId() != id)
            continue;
        // Ownership outlives a dock tab's mount. The current parent chain excludes
        // inactive targets immediately, without rebuilding an index on tab switches.
        for (const UIElement* ancestor = element; ancestor; ancestor = ancestor->GetDfsParent())
        {
            if (ancestor == &subtree)
            {
                if (element == &subtree)
                    return element;
                if (!firstMatch || PrecedesInLookupOrder(*element, *firstMatch))
                    firstMatch = element;
                break;
            }
        }
    }
    return firstMatch;
}

} // namespace GameEngine::UI
