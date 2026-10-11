#include "PathfindingECS/Systems/NavigationPathfindingSystem.h"
#include "PathfindingECS/NavigationService.h"
#include "PathfindingECS/Components/NavigationAgent.h"
#include "PathfindingECS/Components/NavigationAgentState.h"
#include "PathfindingECS/Systems/NavigationWorldHooks.h"
#include "Components/Transform.h"
#include "ECS/ECSTemplates.h"
#include "ECS/Query.h"
#include "Pathfinding/GridMap.h"
#include "Pathfinding/NavigationWorld.h"
#include "Pathfinding/PathBuffer.h"
#include "Pathfinding/PathfindingTypes.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace GameEngine::PathfindingECS
{

namespace
{

// True while every cell of the agent's stamped square footprint is still owned
// by it, still marked as a body and not yet expired. A warm centre alone cannot
// prove a large body, so the whole footprint is tested. Cells outside the map
// are excluded, matching the edge clipping the stamp itself performs.
bool HasCurrentBody(const Pathfinding::GridMap& grid, const Components::NavigationAgent& agent,
                    float32 posX, float32 posZ, float32 currentTime)
{
    uint32 x = 0, z = 0;
    if (!grid.WorldToCell(posX, posZ, x, z))
        return false;

    // WorldToCell succeeded, so the centre is inside the map and Width/Depth are
    // at least one. Clamping the reach to the map first keeps x + radius from
    // wrapping for an agent whose radius dwarfs the grid.
    const Pathfinding::GridSettings& settings = grid.GetSettings();
    const uint32 radius = std::min(grid.FootprintRadiusInCells(agent.Radius),
                                   std::max(settings.Width, settings.Depth));
    const uint32 minX = radius < x ? x - radius : 0;
    const uint32 minZ = radius < z ? z - radius : 0;
    const uint32 maxX = std::min(x + radius, settings.Width - 1);
    const uint32 maxZ = std::min(z + radius, settings.Depth - 1);
    for (uint32 cz = minZ; cz <= maxZ; ++cz)
    {
        for (uint32 cx = minX; cx <= maxX; ++cx)
        {
            const auto cell = grid.GetCellReservation(cx, cz);
            if (cell.AgentId != agent.AgentId || cell.Occupancy == 0 || !cell.IsActive(currentTime))
                return false;
        }
    }
    return true;
}

void CacheWaypointsFromPath(Components::NavigationAgentState& state, const Pathfinding::PathBuffer& pathBuffer)
{
    Pathfinding::PathHandle pathHandle{state.PathIndex, state.PathGeneration};
    uint32 pointCount = pathBuffer.GetPointCount(pathHandle);
    if (pointCount == 0)
        return;

    uint32 currentIdx = state.CurrentPathPointIndex;
    if (currentIdx >= pointCount)
        currentIdx = pointCount - 1;

    Pathfinding::PathPoint current = pathBuffer.GetPoint(pathHandle, currentIdx);
    state.CurrentWaypointX = current.X;
    state.CurrentWaypointY = current.Y;
    state.CurrentWaypointZ = current.Z;

    uint32 nextIdx = currentIdx + 1;
    if (nextIdx < pointCount)
    {
        Pathfinding::PathPoint next = pathBuffer.GetPoint(pathHandle, nextIdx);
        state.NextWaypointX = next.X;
        state.NextWaypointY = next.Y;
        state.NextWaypointZ = next.Z;
    }
    else
    {
        state.NextWaypointX = current.X;
        state.NextWaypointY = current.Y;
        state.NextWaypointZ = current.Z;
    }
}

Pathfinding::PathRequest BuildPathRequest(const Components::NavigationAgent& agent,
                                          const Components::NavigationAgentState& state,
                                          const Components::WorldTransform& wt, float32 currentTime)
{
    // WorldTransform is column-major: [12], [13], [14] hold the translation.
    Pathfinding::PathRequest request;
    request.StartX = wt.matrix[12];
    request.StartY = wt.matrix[13];
    request.StartZ = wt.matrix[14];
    request.EndX = state.DestinationX;
    request.EndY = state.DestinationY;
    request.EndZ = state.DestinationZ;
    request.AgentRadius = agent.Radius;
    request.AgentHeight = agent.Height;
    request.ExcludeAgentId = agent.AgentId;
    request.CurrentTime = currentTime;
    request.SmoothPath = agent.SmoothPaths;
    return request;
}

void ReleasePath(Components::NavigationAgentState& state, Pathfinding::PathBuffer& pathBuffer)
{
    const Pathfinding::PathHandle oldPath{state.PathIndex, state.PathGeneration};
    if (!oldPath.IsValid())
        return;
    pathBuffer.FreePath(oldPath);
    state.PathIndex = 0;
    state.PathGeneration = 0;
}

} // anonymous namespace

// A request is an agent with HasDestination and either PathDirty or
// ReplanRequested set. Game code sets HasDestination and PathDirty (orders,
// abilities, training), and the engine re-dirties a path through the Failed
// retry below and the movement system's corridor checks; the replan timer
// raises ReplanRequested, a refresh that keeps the current path. Each Update
// harvests finished async requests, marks timer and Failed replans, queues
// every request (the first Update it is seen stamps its sequence; a PathDirty
// request is marked Pending and releases its old path), prepares the bodies of
// every agent on a grid, then solves the oldest MaxPathSolvesPerFrame requests.
// The rest stay queued in order for a later Update.
void NavigationPathfindingSystem::Update(ECS::World& world, float32 deltaTime)
{
    auto* navWorld = NavigationService::TryGet();
    if (!navWorld)
        return;

    m_AccumulatedTime += deltaTime;
    ++m_Statistics.Frames;
    auto& pathBuffer = navWorld->GetPathBuffer();
    const uint64 worldId = world.GetWorldId();
    const uint64 resetGeneration = world.GetLifecycleResetGeneration();
    const uint64 serviceGeneration = NavigationService::GetGeneration();

    // Step 1a: Collect all completed results into a vector keyed by entity ID.
    // This avoids O(N*M) by decoupling task harvesting from entity queries.
    m_CompletedResults.clear();

    for (auto it = m_InFlightRequests.begin(); it != m_InFlightRequests.end();)
    {
        const auto& pending = it->second;
        if (!pending.Task.IsDone())
        {
            ++it;
            continue;
        }

        PendingCompletion completion;
        completion.EntityId = it->first;
        completion.Success = pending.Task.TryGetResult<Pathfinding::PathResult>(completion.Result);
        if (pending.WorldId != worldId || pending.ResetGeneration != resetGeneration ||
            pending.ServiceGeneration != serviceGeneration)
        {
            // Clear restarts entity generations; another World can also have
            // the same handle. Retire the old result without touching an actor.
            // A replaced service already destroyed its own PathBuffer: its
            // numeric path handles must never be freed in the new buffer.
            if (pending.ServiceGeneration == serviceGeneration && completion.Success &&
                completion.Result.Path.IsValid())
                pathBuffer.FreePath(completion.Result.Path);
            it = m_InFlightRequests.erase(it);
            continue;
        }
        m_CompletedResults.push_back(std::move(completion));
        it = m_InFlightRequests.erase(it);
    }

    // Step 1b: Single query pass to write back all completed results
    if (!m_CompletedResults.empty())
    {
        world.Query<ECS::Read<Components::NavigationAgent>,
                    ECS::Write<Components::NavigationAgentState>>().Each(
            [&](ECS::EntityHandle e, const Components::NavigationAgent& /*agent*/,
                Components::NavigationAgentState& state)
            {
                uint64 entityKey = static_cast<uint64>(e.id);
                const PendingCompletion* found = nullptr;
                for (const auto& entry : m_CompletedResults)
                {
                    if (entry.EntityId == entityKey)
                    {
                        found = &entry;
                        break;
                    }
                }
                if (!found)
                    return;

                // A timer refresh walked its old path until now.
                ReleasePath(state, pathBuffer);
                state.ReplanRequested = false;
                if (found->Success)
                {
                    state.PathIndex = found->Result.Path.Index;
                    state.PathGeneration = found->Result.Path.Generation;
                    state.Status = static_cast<uint8>(found->Result.Status);
                    state.CurrentPathPointIndex = 0;
                    state.PathDirty = false;

                    if (found->Result.Status == Pathfinding::PathStatus::Complete ||
                        found->Result.Status == Pathfinding::PathStatus::Partial)
                    {
                        CacheWaypointsFromPath(state, pathBuffer);
                    }
                }
                else
                {
                    state.Status = static_cast<uint8>(Pathfinding::PathStatus::Failed);
                    state.PathDirty = false;
                    Logger::Log::Warning("[PathfindingECS] Path request failed for entity {}", e.id);
                }
            });

        // Free path handles for completed requests whose entities no longer exist.
        // These are orphaned paths that would otherwise leak in the PathBuffer.
        for (const auto& entry : m_CompletedResults)
        {
            if (entry.Success && entry.Result.Path.IsValid())
            {
                // Check if this completion was consumed by the entity query above.
                // If the entity was destroyed, the query never matched it — free the path.
                bool consumed = false;
                world.Query<ECS::Read<Components::NavigationAgentState>>().Each(
                    [&](ECS::EntityHandle e, const Components::NavigationAgentState& state)
                    {
                        if (consumed)
                            return;
                        if (static_cast<uint64>(e.id) == entry.EntityId &&
                            state.PathIndex == entry.Result.Path.Index &&
                            state.PathGeneration == entry.Result.Path.Generation)
                        {
                            consumed = true;
                        }
                    });
                if (!consumed)
                {
                    pathBuffer.FreePath(entry.Result.Path);
                }
            }
        }
    }

    // Step 1.5: Re-evaluation - periodically mark agents for re-pathing. The
    // marks are not limited here: every request, whatever marked it, shares
    // the solve budget below. Timers are staggered on first init so agents
    // spread their replans across frames. A timer does not re-arm while its
    // request waits; it keeps counting down and fires again once the
    // request is solved.
    world.Query<ECS::Read<Components::NavigationAgent>,
                ECS::Write<Components::NavigationAgentState>>().Each(
        [&](ECS::EntityHandle e, const Components::NavigationAgent& agent,
            Components::NavigationAgentState& state)
        {
            if (!state.HasDestination)
                return;

            auto status = static_cast<Pathfinding::PathStatus>(state.Status);
            // Retry Failed even when ReplanInterval is 0 (a game may set it) so the
            // agent moves once a blocked dest or missing map clears. Occupancy
            // is a path tax, not a Failed.
            if (status == Pathfinding::PathStatus::Failed)
            {
                if (state.ReplanTimer <= 0.0f && !state.PathDirty)
                {
                    state.PathDirty = true;
                    state.ReplanTimer = kFailedPathRetrySeconds;
                }
                else
                    state.ReplanTimer -= deltaTime;
                return;
            }

            if (agent.ReplanInterval <= 0.0f)
                return;
            if (status != Pathfinding::PathStatus::Complete &&
                status != Pathfinding::PathStatus::Partial)
                return;

            // Stagger timer initialization: on first tick (timer == 0 and path
            // is active), set the timer to a fraction of the interval based on a
            // simple hash of the agent's ID. This spreads replans across frames
            // instead of all agents replanning simultaneously.
            if (state.ReplanTimer <= 0.0f && !state.PathDirty && !state.ReplanRequested)
            {
                state.ReplanRequested = true;

                // Stagger the next replan using a cheap hash to distribute across frames.
                // Use AgentId if set, otherwise fall back to entity ID to avoid all-zero hash.
                uint32 hashInput = (agent.AgentId != 0) ? agent.AgentId : static_cast<uint32>(e.id);
                uint32 hash = hashInput * 2654435761u; // Knuth multiplicative hash
                float32 jitter = static_cast<float32>(hash & 0xFFu) / 255.0f; // 0..1
                state.ReplanTimer = agent.ReplanInterval * (0.5f + 0.5f * jitter);
            }
            else
            {
                state.ReplanTimer -= deltaTime;
            }
        });

    // Bodies are prepared after queuing, which changes no input the
    // preparation reads, and before the first grid solve, so every solve in
    // a frame sees the same occupancy.
    if (CollectRequests(world))
        PrepareBodies(world);

    SolveOldestRequests(world);
}

// Queues every request in query order and reports whether any targets a grid
// map. A request seen for the first time takes the next sequence. A PathDirty
// request becomes Pending and releases its old path, the state it waits in
// until it is solved, including a timer refresh that a corridor check
// re-dirtied while it waited; a timer refresh alone keeps its path, except on an
// invalid map, where it becomes a dirty request.
bool NavigationPathfindingSystem::CollectRequests(ECS::World& world)
{
    auto& navWorld = NavigationService::Get();
    auto& pathBuffer = navWorld.GetPathBuffer();
    m_Queue.clear();
    m_Statistics.NewRequests = 0;
    bool hasGridRequest = false;
    world.Query<ECS::Read<Components::NavigationAgent>,
                ECS::Write<Components::NavigationAgentState>,
                ECS::Read<Components::WorldTransform>>().Each(
        [&](ECS::EntityHandle e, const Components::NavigationAgent& agent,
            Components::NavigationAgentState& state, const Components::WorldTransform& wt)
        {
            if (!state.HasDestination || (!state.PathDirty && !state.ReplanRequested))
            {
                // A request withdrawn while queued (arrival, a game clearing
                // PathDirty) gives up its place.
                if (state.PathRequestSequence != 0)
                    state.PathRequestSequence = 0;
                if (!state.HasDestination)
                    state.ReplanRequested = false;
                return;
            }

            const Pathfinding::NavMapHandle map{agent.NavMapIndex, agent.NavMapGeneration};
            if (!hasGridRequest)
                hasGridRequest = navWorld.GetGridMap(map) != nullptr;

            if (state.PathDirty && state.Status != static_cast<uint8>(Pathfinding::PathStatus::Pending))
            {
                ReleasePath(state, pathBuffer);
                state.Status = static_cast<uint8>(Pathfinding::PathStatus::Pending);
            }

            // An async request in flight replaces the path when it completes,
            // or is retired once its scope has moved on.
            if (m_InFlightRequests.count(static_cast<uint64>(e.id)) > 0)
                return;

            // With no map to solve on, a request waits unqueued as a Pending
            // PathDirty request, without a path, until the agent has a map
            // again; a timer refresh becomes one.
            if (!map.IsValid())
            {
                ReleasePath(state, pathBuffer);
                state.PathDirty = true;
                state.ReplanRequested = false;
                state.Status = static_cast<uint8>(Pathfinding::PathStatus::Pending);
                state.PathRequestSequence = 0;
                return;
            }

            if (state.PathRequestSequence == 0)
            {
                state.PathRequestSequence = IssueSequence();
                ++m_Statistics.NewRequests;
            }
            m_Queue.push_back({e, state.PathRequestSequence, map,
                               BuildPathRequest(agent, state, wt, m_AccumulatedTime)});
        });
    m_Statistics.TotalNewRequests += m_Statistics.NewRequests;
    return hasGridRequest;
}

// Stamps the bodies a solve must see. NavigationMovementSystem owns ordinary
// frame-to-frame stamps; this restores the ones a request or a missing stamp
// left out.
void NavigationPathfindingSystem::PrepareBodies(ECS::World& world)
{
    auto& navWorld = NavigationService::Get();
    auto& pathBuffer = navWorld.GetPathBuffer();
    world.Query<ECS::Read<Components::NavigationAgent>,
                ECS::Write<Components::NavigationAgentState>,
                ECS::Read<Components::WorldTransform>>().Each(
        [&](ECS::EntityHandle, const Components::NavigationAgent& agent,
            Components::NavigationAgentState& state, const Components::WorldTransform& wt)
        {
            if (agent.AgentId == 0)
                return;
            auto* grid = navWorld.GetGridMap({agent.NavMapIndex, agent.NavMapGeneration});
            if (!grid)
                return;

            const bool requesting = state.HasDestination && state.PathDirty;
            const auto status = static_cast<Pathfinding::PathStatus>(state.Status);
            if (state.HasDestination && !state.PathDirty &&
                (status == Pathfinding::PathStatus::Complete || status == Pathfinding::PathStatus::Partial) &&
                state.CurrentPathPointIndex < pathBuffer.GetPointCount({state.PathIndex, state.PathGeneration}))
                return; // Preserve the native moving body's complete lookahead corridor.

            // A dirty replanner deliberately drops its old corridor once.
            // Idle/direct peers need rebuilding only if actual body cells
            // are missing, expired or at their previous position. The
            // HasCellReservations hint can outlive its expired cells.
            if (!requesting && HasCurrentBody(*grid, agent, wt.matrix[12], wt.matrix[14], m_AccumulatedTime))
                return;
            StampNavigationAgentOccupancy(agent, state, wt.matrix[12], wt.matrix[14], m_AccumulatedTime);
        });
}

// Solves the oldest MaxPathSolvesPerFrame queued requests, oldest first, and
// records what the budget left waiting.
void NavigationPathfindingSystem::SolveOldestRequests(ECS::World& world)
{
    auto& navWorld = NavigationService::Get();
    auto& pathBuffer = navWorld.GetPathBuffer();
    const uint64 worldId = world.GetWorldId();
    const uint64 resetGeneration = world.GetLifecycleResetGeneration();
    const uint64 serviceGeneration = NavigationService::GetGeneration();

    const auto older = [this](const QueuedRequest& a, const QueuedRequest& b)
    { return SequenceAge(a.Sequence) > SequenceAge(b.Sequence); };
    const std::size_t solveCount = std::min<std::size_t>(std::max(MaxPathSolvesPerFrame, 1u), m_Queue.size());
    std::partial_sort(m_Queue.begin(), m_Queue.begin() + static_cast<std::ptrdiff_t>(solveCount), m_Queue.end(),
                      older);

    m_Statistics.Solved = 0;
    m_Statistics.LongestSolvedWaitFrames = 0;
    m_Statistics.LongestSolvedWaitSeconds = 0.0f;
    for (std::size_t i = 0; i < solveCount; ++i)
    {
        const QueuedRequest& queued = m_Queue[i];
        // Cannot be null inside one Update (no structural change since the
        // queue was collected), but a stale handle must never be written.
        auto* state = world.GetComponentForWrite<Components::NavigationAgentState>(queued.Entity);
        if (!state)
            continue;

        uint32 waitFrames = 0;
        float32 waitSeconds = 0.0f;
        ReadWait(queued.Sequence, waitFrames, waitSeconds);
        m_Statistics.LongestSolvedWaitFrames = std::max(m_Statistics.LongestSolvedWaitFrames, waitFrames);
        m_Statistics.LongestSolvedWaitSeconds = std::max(m_Statistics.LongestSolvedWaitSeconds, waitSeconds);
        state->PathRequestSequence = 0;
        ++m_Statistics.Solved;

        auto taskHandle = navWorld.RequestPath(queued.Map, queued.Request);
        if (taskHandle.IsValid())
        {
            m_InFlightRequests[static_cast<uint64>(queued.Entity.id)] = {
                std::move(taskHandle), worldId, resetGeneration, serviceGeneration};
            continue;
        }

        // Synchronous fallback
        Pathfinding::PathHandle outPath;
        const Pathfinding::PathStatus status = navWorld.FindPath(queued.Map, queued.Request, outPath);
        ReleasePath(*state, pathBuffer);
        state->ReplanRequested = false;
        state->PathIndex = outPath.Index;
        state->PathGeneration = outPath.Generation;
        state->Status = static_cast<uint8>(status);
        state->CurrentPathPointIndex = 0;
        state->PathDirty = false;
        if (status == Pathfinding::PathStatus::Complete || status == Pathfinding::PathStatus::Partial)
            CacheWaypointsFromPath(*state, pathBuffer);
    }
    m_Statistics.TotalSolved += m_Statistics.Solved;

    m_Statistics.Backlog = static_cast<uint32>(m_Queue.size() - solveCount);
    m_Statistics.OldestWaitFrames = 0;
    m_Statistics.OldestWaitSeconds = 0.0f;
    if (m_Statistics.Backlog == 0)
    {
        m_SequenceIssues.clear();
        return;
    }
    const auto oldest = std::min_element(m_Queue.begin() + static_cast<std::ptrdiff_t>(solveCount), m_Queue.end(),
                                         older);
    ReadWait(oldest->Sequence, m_Statistics.OldestWaitFrames, m_Statistics.OldestWaitSeconds);
    // Issues older than the one holding the oldest queued request are no
    // longer read.
    if (!m_SequenceIssues.empty())
        m_SequenceIssues.erase(m_SequenceIssues.begin(),
                               m_SequenceIssues.begin() + static_cast<std::ptrdiff_t>(FindIssue(oldest->Sequence)));
}

uint32 NavigationPathfindingSystem::IssueSequence()
{
    const uint32 sequence = m_NextSequence;
    m_NextSequence = m_NextSequence == std::numeric_limits<uint32>::max() ? 1u : m_NextSequence + 1u;
    if (m_SequenceIssues.empty() || m_SequenceIssues.back().Frame != m_Statistics.Frames)
        m_SequenceIssues.push_back({sequence, m_Statistics.Frames, m_AccumulatedTime});
    return sequence;
}

// The index of the issue that holds `sequence`: the newest one issued at or
// before it. A sequence older than every issue kept (an agent that left the
// query while queued and came back) maps to the oldest issue. The list must
// not be empty.
std::size_t NavigationPathfindingSystem::FindIssue(uint32 sequence) const
{
    const uint32 age = SequenceAge(sequence);
    for (std::size_t i = m_SequenceIssues.size(); i > 0; --i)
    {
        if (SequenceAge(m_SequenceIssues[i - 1].FirstSequence) >= age)
            return i - 1;
    }
    return 0;
}

void NavigationPathfindingSystem::ReadWait(uint32 sequence, uint32& frames, float32& seconds) const
{
    frames = 0;
    seconds = 0.0f;
    if (m_SequenceIssues.empty())
        return;
    const SequenceIssue& issue = m_SequenceIssues[FindIssue(sequence)];
    frames = static_cast<uint32>(m_Statistics.Frames - issue.Frame);
    seconds = m_AccumulatedTime - issue.Time;
}

} // namespace GameEngine::PathfindingECS
