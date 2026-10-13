#pragma once

#include "Types/StringId.h"
#include "Types/Types.h"

#include <string_view>
#include <vector>

namespace GameEngine
{
class UIElement;

namespace UI
{

/// UI-thread-only authored-id index owned by a UIManager. Elements unregister before
/// destruction or an owner/id change. Mount reachability is checked at lookup time.
class ElementIdIndex
{
  public:
    void Register(UIElement& element);
    void Unregister(UIElement& element);

    /// Find the first matching id in depth-first order, including subtree itself.
    /// Ordinary children precede a Mount target, independent of registration order.
    UIElement* Find(const UIElement& subtree, std::string_view id) const;

  private:
    FastHashMap<StringId, std::vector<UIElement*>> m_Elements;
};

} // namespace UI
} // namespace GameEngine
