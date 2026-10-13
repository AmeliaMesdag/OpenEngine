#pragma once

#include "Types/StringId.h"
#include "Types/Types.h"

#include <vector>

namespace GameEngine
{
class UIElement;

namespace UI
{

/// UI-thread-only authored-id index owned by a UIManager, keyed by StringId. Elements
/// unregister before destruction or an owner/id change. Mount reachability is checked at
/// lookup time. Two different ids with the same StringId are refused at registration
/// (Debug asserts, Release logs an error naming both), so a lookup compares no strings.
/// A refused id stays unregistered, even after the element it collided with is destroyed,
/// until its element registers again (an id or owner change).
class ElementIdIndex
{
  public:
    void Register(UIElement& element);
    void Unregister(UIElement& element);

    /// Find the first matching id in depth-first order, including subtree itself.
    /// Ordinary children precede a Mount target, independent of registration order.
    /// Returns nullptr for id 0: an empty id is never registered.
    UIElement* Find(const UIElement& subtree, StringId id) const;

  private:
    FastHashMap<StringId, std::vector<UIElement*>> m_Elements;
};

} // namespace UI
} // namespace GameEngine
