#pragma once

namespace GameEngine
{

class EditorDebugServer;

// Navigation's live state: what the path solve budget did in the newest frame and in total.
void RegisterNavigationDebugHandlers(EditorDebugServer& server);

} // namespace GameEngine
