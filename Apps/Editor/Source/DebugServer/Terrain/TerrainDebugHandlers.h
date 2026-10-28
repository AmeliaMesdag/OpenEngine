#pragma once

namespace GameEngine
{

class EditorApplication;
class EditorDebugServer;

void RegisterTerrainDebugHandlers(EditorDebugServer& server, EditorApplication& app);

/// Registers planet_page_overlay: a raised-cosine bump on one face of the active planet's height
/// pages (TerrainService::SetPlanetPageOverlay), or its removal.
void RegisterPlanetPageOverlayHandler(EditorDebugServer& server);

} // namespace GameEngine
