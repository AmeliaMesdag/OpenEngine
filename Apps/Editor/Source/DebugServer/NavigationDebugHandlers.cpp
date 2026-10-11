#include "DebugServer/NavigationDebugHandlers.h"

#include "DebugServer/DebugServerReply.h"
#include "DebugServer/EditorDebugServer.h"

#include "Core/Engine.h"
#include "ECS/Systems.h"
#include "ECSModules/Rendering/RenderingLoop.h"
#include "PathfindingECS/Systems/NavigationPathfindingStatistics.h"
#include "PathfindingECS/Systems/NavigationPathfindingSystem.h"

#include <nlohmann/json.hpp>

namespace GameEngine
{

using json = nlohmann::json;

namespace
{

json HandleNavigationStats(const EditorDebugServer::RequestContext&)
{
    auto* loop = EngineCore::GetInstance().GetRenderingLoop();
    auto* systems = loop ? loop->GetSystemManager() : nullptr;
    if (!systems)
        return Editor::RefuseRequest("No ECS system manager is running (the rendering loop has not started)");
    const auto* pathfinding = systems->GetSystem<PathfindingECS::NavigationPathfindingSystem>();
    if (!pathfinding)
        return Editor::RefuseRequest(
            "NavigationPathfindingSystem is not in the running schedule (the PathfindingECS systems are not registered)");

    const PathfindingECS::NavigationPathfindingStatistics& stats = pathfinding->GetStatistics();
    return json{
        {"maxPathSolvesPerFrame", pathfinding->MaxPathSolvesPerFrame},
        {"frames", stats.Frames},
        {"frame",
         {{"newRequests", stats.NewRequests},
          {"solved", stats.Solved},
          {"backlog", stats.Backlog},
          {"oldestWaitFrames", stats.OldestWaitFrames},
          {"oldestWaitSeconds", stats.OldestWaitSeconds},
          {"longestSolvedWaitFrames", stats.LongestSolvedWaitFrames},
          {"longestSolvedWaitSeconds", stats.LongestSolvedWaitSeconds}}},
        {"totals", {{"newRequests", stats.TotalNewRequests}, {"solved", stats.TotalSolved}}},
    };
}

} // namespace

void RegisterNavigationDebugHandlers(EditorDebugServer& server)
{
    // get_navigation_stats — NavigationPathfindingSystem's solve budget (NavigationPathfindingStatistics).
    // Answered on the main thread between frames (the debug server's request pump), so it reads the
    // counters of a finished Update. No parameters. Reply:
    //   maxPathSolvesPerFrame: the budget, solves one frame may start.
    //   frames: Updates the system has run.
    //   frame: the newest Update: newRequests (requests that joined the queue), solved, backlog
    //     (requests left queued), oldestWaitFrames / oldestWaitSeconds (the oldest request still
    //     queued; 0 when none is) and longestSolvedWaitFrames / longestSolvedWaitSeconds (the longest
    //     wait among the requests it solved). A request solved in the frame it joined waited 0.
    //   totals: newRequests and solved since the system was created; read twice and subtract for a rate.
    server.RegisterHandler("get_navigation_stats", HandleNavigationStats);
}

} // namespace GameEngine
