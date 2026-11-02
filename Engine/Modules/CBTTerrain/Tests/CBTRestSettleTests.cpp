// The tree a still camera settles to is a function of the pose, not of the path the camera took.
//
// Merges keep a hysteresis band while the camera moves: a pair merges only once its parents' demand is
// well under the split threshold, so a moving boundary does not churn. Every leaf inside that band is
// a valid rest state, so a camera that arrives from close by keeps finer leaves than one that arrives
// from far away. While the inputs hold still the update applies the rest rule
// (CBTClassifyDesc::Settle, CBT_PlanarClassify): the split and the merge use one threshold, and the
// planar demand is monotone in depth (a child never demands more than its parent), so the only tree
// that holds still is the one the pose asks for. These tests drive the kernels the way CBTRenderFeature
// does: the first update at a new pose has Settle 0, every later one at that pose Settle 1.
//
// Instrument: the live heap ids read back from the GPU after the tree is quiet, compared as a sorted
// set. Two arrivals settle to one tree when the sets are equal, not merely the counts.
//
// The pool-pressure controllers (the off-frustum keep step, the near-bias contention) keep state across a path.
// A test turns them off or stays below where they engage, so the rest rule alone is under test, or it puts the
// pool under the one controller it tests (AContendedPoolGoesQuietAtRest, APoolUnderTheKeepStepGoesQuietAtRest).
//
// The screen-space edge rule, which decides the planar terrains without a height pyramid, keeps its motion
// band at rest; the TheEdgeRule tests bind the relief through the atlas source and assert that it goes
// quiet and that the band holds under a small move.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "CBTTerrain/CBTDemandTuning.h"
#include "CBTTerrain/CBTInstance.h"
#include "CBTTerrain/CBTKernelSet.h"
#include "CBTTerrain/CBTLayout.h"
#include "CBTTerrain/CBTNearBias.h"
#include "CBTTerrain/CBTTreeAudit.h"
#include "CBTDiamond.h"
#include "CBTReliefHeightSource.h"
#include "CBTTestHarness.h"
#include "Mathematics/MatrixOps.h"
#include "Mathematics/Vector3.h"
#include "Rendering/Core/CommandList.h"

using namespace GameEngine::CBTTerrain;
using namespace GameEngine::CBTTerrain::Test;
using namespace GameEngine::Rendering;

namespace
{
constexpr float kScreenW = 1600.0f;
constexpr float kScreenH = 900.0f;
constexpr float kTerrainSizeM = 512.0f;
constexpr float kHeightScale = 60.0f; // the shipped ComposedIsland relief
constexpr uint32_t kReliefDim = 512u;
constexpr uint32_t kMaxDepth = 21u;
// A split threshold that keeps both rest poses under the contention ramp.
constexpr float kSplitPx = 20.0f;
// A split threshold that fills the pool at the walk pose.
constexpr float kFullPoolSplitPx = 6.0f;
// A split threshold that keeps the walk pose's rest tree on the near-bias contention ramp, between its
// foot (0.50) and its top (0.90), where the contention moves with the live count.
constexpr float kContendedSplitPx = 12.0f;
// Updates per stage. A stage converges in well under this many (the parent-metric suite converges in
// 140); the rest stage then has to stay quiet for kQuietUpdates more.
constexpr uint32_t kStageUpdates = 160u;
constexpr uint32_t kQuietUpdates = 8u;
// The fraction of the largest live count the smallest must reach over the motion stage's second half,
// where the camera holds still and the tree has converged.
constexpr float kStillLiveBand = 0.98f;
// The updates a held stage under the keep step may take to stop changing, out of kStageUpdates.
constexpr uint32_t kKeepSettleUpdates = 120u;
// The updates the turned and back stages of both keep-path poses may take together to stop changing.
constexpr uint32_t kStopSettleUpdates = 150u;
// The pressure step the gate test's arrival climbs to in motion: one notch survives the update that drains
// the pool under the keep ceiling, so the rest starts above pressure step 0.
constexpr int32_t kArrivalPressureStep = 2;
// The contention ramp's lower bound (cbt_layout.glsl CBT_NEAR_BIAS_CONTENTION_LO): below it the
// near-bias is exactly 1.
constexpr float kContentionFreeOccupancy = 0.50f;
// The rest updates the edge rule may take to go quiet after a converged motion tree. At rest the edge
// rule keeps the motion thresholds, so a converged motion tree is already a rest tree; the bound catches
// a rest rule that keeps changing the tree.
constexpr uint32_t kEdgeRuleRestUpdates = 24u;
// A camera that moves 1 cm and back on alternate updates: far inside the motion band of every facet.
constexpr float kJiggleM = 0.01f;
constexpr uint32_t kJiggleUpdates = 48u;
constexpr uint32_t kJiggleCounted = 32u;

struct Pose
{
    float EyeX, EyeY, EyeZ;
    float YawDeg, PitchDeg;
};

// Two rest poses with an approach from close by (the tree arrives finer than the pose needs) and one
// from far away (it arrives coarser). The overview looks across the relief from above it, so the domain
// metric owns most of the tree; the walk stands 2 m above the ground looking along it, so the near-field
// bound owns the ground under the eye.
struct Arrival
{
    const char* Label;
    Pose Rest;
    Pose FromNear;
    Pose FromFar;
};
// The terrain spans [0, 512] m in X and Z; the eye heights are meters above the relief under the eye.
float GroundY(float x, float z)
{
    return ReliefHeightAt(x / kTerrainSizeM, z / kTerrainSizeM) * kHeightScale;
}

Pose Above(float x, float aboveM, float z, float yawDeg, float pitchDeg)
{
    return {x, GroundY(x, z) + aboveM, z, yawDeg, pitchDeg};
}

const Arrival kArrivals[] = {
    {"overview", Above(96.0f, 60.0f, 96.0f, 45.0f, -20.0f), Above(176.0f, 15.0f, 176.0f, 45.0f, -35.0f),
     Above(96.0f, 360.0f, 96.0f, 45.0f, -20.0f)},
    {"walk", Above(256.0f, 2.0f, 64.0f, 0.0f, -4.0f), Above(256.0f, 2.0f, 100.0f, 0.0f, -10.0f),
     Above(256.0f, 302.0f, 64.0f, 0.0f, -4.0f)},
};

// The demand shaping a test runs with. The default is the shipped set at kSplitPx; the pool-pressure tests
// turn single controllers off so the one under test is the only reader of the pool's occupancy.
struct Shaping
{
    float SplitPx = kSplitPx;
    bool NearBias = true;               // the near-bias contention ramp
    float KeepOcc = kOffFrustumKeepOcc; // 0 leaves the planar off-frustum field ungated (no keep step)
    uint32_t PoolPressure = 1u;         // 0 holds the pool-pressure scale at 1x
    bool EdgeRule = false;              // the relief through the atlas source, which keeps the edge rule
};

void BuildParams(CBTFrameParams& p, const Pose& pose, const Shaping& shaping = {})
{
    using namespace GameEngine::Mathematics;
    p = CBTFrameParams{};
    p.Screen[0] = kScreenW;
    p.Screen[1] = kScreenH;
    p.Screen[2] = shaping.SplitPx;
    p.Screen[3] = 0.5f * shaping.SplitPx; // CBTRenderFeature ships merge = split / 2
    p.TerrainSize[0] = kTerrainSizeM;
    p.TerrainSize[1] = kTerrainSizeM;
    p.TerrainSize[2] = kHeightScale;
    p.TerrainSize[3] = 0.0f;
    p.TerrainOrigin[2] = static_cast<float>(kMaxDepth);

    constexpr float kDegToRad = 3.14159265f / 180.0f;
    const float yaw = pose.YawDeg * kDegToRad, pitch = pose.PitchDeg * kDegToRad;
    const Vector3 eye(pose.EyeX, pose.EyeY, pose.EyeZ);
    const Vector3 dir(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    const Matrix4x4 proj = MakePerspectiveLH_ZO_ReverseZ(1.05f, kScreenW / kScreenH, 0.5f, 20000.0f);
    const Matrix4x4 vp = proj * MakeLookAtLH(eye, eye + dir, Vector3(0, 1, 0));
    std::memcpy(p.ViewProjRel, &vp.GetGLM()[0][0], sizeof(p.ViewProjRel));
    p.CameraPos[0] = eye.x;
    p.CameraPos[1] = eye.y;
    p.CameraPos[2] = eye.z;
    p.CameraPos[3] = 1.0f;

    // The shipped demand shaping: near-bias, near-field target, off-frustum keep, edge rescue.
    const CBTNearBiasRadii nb = ComputeNearBiasRadii(pose.EyeY);
    p.NearBias[0] = shaping.NearBias ? 1.0f : 0.0f;
    p.NearBias[1] = nb.NearRadius;
    p.NearBias[2] = nb.FarRadius;
    p.NearBias[3] = kNearBiasMaxCoarsen;
    p.DemandTuning[0] = kNearFieldFacetTargetM;
    p.DemandTuning[1] = shaping.KeepOcc;
    p.DemandTuning[2] = kEdgeRescueTpeMul * shaping.SplitPx;
    p.DemandTuning[3] = kEdgeRescueOcc;
}

// The relief a test runs on: the unified height texture, where the planar criterion decides, or with
// Shaping::EdgeRule the same relief through the atlas source, where the screen-space edge rule decides.
class ShapedRelief
{
  public:
    ShapedRelief(IDevice& device, CBTInstance& instance, const Shaping& shaping)
    {
        if (shaping.EdgeRule)
            m_Atlas.emplace(device, instance, kReliefDim + 1u);
        else
            m_Unified.emplace(device, instance, kHeightScale, kReliefDim);
    }

    // Whether the relief's height source is bound. Unbound, the lattice is flat.
    bool IsArmed() const { return m_Atlas ? m_Atlas->IsArmed() : m_Unified && m_Unified->IsArmed(); }

    // Writes the atlas geometry into `p` when the relief comes through the atlas.
    void Apply(CBTFrameParams& p) const
    {
        if (m_Atlas)
            m_Atlas->Apply(p);
    }

  private:
    std::optional<ReliefHeightSource> m_Unified;
    std::optional<ReliefAtlasSource> m_Atlas;
};

// A motion stage then a rest stage at one pose: the live count the motion converged to, the lowest live
// count any rest update left, and whether the last kQuietUpdates rest updates split and merged nothing.
struct RestTrace
{
    uint32_t MotionLive = 0;
    uint32_t RestLiveMin = 0;
    uint32_t RestLiveEnd = 0;
    uint32_t StillLiveMin = std::numeric_limits<uint32_t>::max(); // the live count over the motion stage's second half
    uint32_t StillLiveMax = 0;
    uint32_t RestStillMin = std::numeric_limits<uint32_t>::max(); // the live count over the rest stage's second half
    uint32_t RestStillMax = 0;
    int32_t KeepStepMax = 0;            // the highest off-frustum keep step over both stages
    uint32_t RestUpdatesChanging = 0; // the rest updates up to the last one that split or merged anything
    bool Quiet = false;
};

// One stage of a held camera: where the tree rests and how long it took.
struct StageRest
{
    uint32_t Live = 0;
    int32_t KeepStep = 0;
    uint32_t UpdatesChanging = 0; // the stage's updates up to the last one that split or merged anything
    uint32_t SplitRuns = 0;       // the runs of consecutive updates that split anything
    bool Quiet = false;           // the last kQuietUpdates updates split and merged nothing
    int32_t ArrivalPressureStep = 0;       // the pressure step after the stage's moving update
    uint32_t RestPressureClimbs = 0;       // the rest updates that raised the pressure step
    uint32_t DownStepsAbovePressureZero = 0; // the rest updates that lowered the keep step after an
                                             // update that ran above pressure step 0
    int32_t ArrivalMergeDemand = 0;      // the merges the stage's moving update asked for
    int32_t ArrivalMergeCandidates = 0;  // the merge candidates the moving update counted
    int32_t RestMergeCandidatesMax = 0;  // the most merge candidates one rest update counted
};

// A keep-path walk: rest at a pose, turn the camera 180 degrees, turn back.
struct PathStages
{
    StageRest Rest;
    StageRest Turned;
    StageRest Back;
};

// The keep-path poses, under kFullPoolSplitPx: each comes under the keep step at rest, and its turned
// view rests at another keep step.
const Pose kKeepPathPoses[] = {Above(256.0f, 6.0f, 256.0f, 0.0f, -30.0f), Above(256.0f, 6.0f, 300.0f, 45.0f, -30.0f)};

Pose Turned(const Pose& pose)
{
    Pose turned = pose;
    turned.YawDeg += 180.0f;
    return turned;
}

struct Settled
{
    std::vector<uint64_t> HeapIds; // sorted live heap ids
    std::vector<uint32_t> States;  // the classify state of each, in the same order
    CBTTessellationStats Stats{};
    bool Quiet = false; // the last kQuietUpdates rest updates split and merged nothing
};
} // namespace

class CBTRestSettle : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_Device = MakeHeadlessDevice();
        if (!m_Device)
            GTEST_SKIP() << "no headless Vulkan device";
        if (!m_Device->GetCapabilities().supportsShaderInt64)
            GTEST_SKIP() << "device lacks shaderInt64";
        if (!m_KernelSet.Initialize(*m_Device, ShaderOutputDir()))
            GTEST_SKIP() << "cbt_kernels.comp.spv missing (glslc unavailable at build)";
    }
    void TearDown() override
    {
        m_KernelSet.Shutdown();
        if (m_Device)
            m_Device->Shutdown();
    }

    void RunUpdate(CBTInstance& inst, const CBTClassifyDesc& desc, const CBTFrameParams& params)
    {
        auto cl = m_Device->CreateCommandList(IDevice::QueueType::Graphics);
        cl->Begin();
        inst.RecordUpdate(*cl, desc, params, m_Frame++);
        cl->End();
        std::vector<CommandList*> lists{cl.get()};
        m_Device->ExecuteCommandLists(lists);
        m_Device->WaitForIdle();
    }

    // Holds the camera at `pose` for `updates` updates as the render feature would: the first update
    // after the move records with Settle 0 (its inputs changed), every later one with Settle 1.
    // Returns whether the last kQuietUpdates split and merged nothing.
    static CBTClassifyDesc ScreenClassify(const Shaping& shaping)
    {
        CBTClassifyDesc c{};
        c.Mode = kClassifyScreenSpace;
        c.TargetDepth = kMaxDepth;
        c.NearFieldGate = 1u;
        c.PoolPressure = shaping.PoolPressure;
        return c;
    }

    bool Hold(CBTInstance& inst, const Pose& pose, uint32_t updates)
    {
        CBTClassifyDesc c = ScreenClassify(Shaping{});
        CBTFrameParams p;
        BuildParams(p, pose);
        uint32_t quiet = 0;
        for (uint32_t i = 0; i < updates; ++i)
        {
            c.Settle = i == 0u ? 0u : 1u;
            RunUpdate(inst, c, p);
            const CBTTessellationStats s = inst.ReadTessellationStats();
            quiet = (s.SplitServed == 0 && s.MergeServed == 0) ? quiet + 1u : 0u;
        }
        return quiet >= kQuietUpdates;
    }

    // `corners`, when given, receives the settled tree with its corners.
    Settled Arrive(const Pose& start, const Pose& rest, CBTTreeSnapshot* corners = nullptr)
    {
        CBTInstance inst;
        EXPECT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
        EXPECT_TRUE(inst.InitializeRoots(kDomainPlanar));
        ReliefHeightSource relief(*m_Device, inst, kHeightScale, kReliefDim);
        EXPECT_TRUE(relief.IsArmed());
        Hold(inst, start, kStageUpdates);
        Settled out;
        out.Quiet = Hold(inst, rest, kStageUpdates);
        out.Stats = inst.ReadTessellationStats();
        if (corners != nullptr)
            *corners = ReadTreeSnapshot(inst, true);
        const std::vector<uint32_t> words = inst.DebugReadWords(CBTBinding::HeapID, kDefaultBisectorPoolSize * 2u);
        constexpr uint32_t kBisectorWords = sizeof(CBTBisectorData) / 4u;
        constexpr uint32_t kStateWord = 1u; // CBTBisectorData::BisectorState
        const std::vector<uint32_t> bisectors =
            inst.DebugReadWords(CBTBinding::BisectorData, kDefaultBisectorPoolSize * kBisectorWords);
        std::vector<std::pair<uint64_t, uint32_t>> live;
        for (uint32_t s = 0; s < kDefaultBisectorPoolSize; ++s)
        {
            const uint64_t h = static_cast<uint64_t>(words[s * 2u]) | (static_cast<uint64_t>(words[s * 2u + 1u]) << 32);
            if (h != 0u)
                live.emplace_back(h, bisectors[s * kBisectorWords + kStateWord]);
        }
        std::sort(live.begin(), live.end());
        for (const auto& [h, state] : live)
        {
            out.HeapIds.push_back(h);
            out.States.push_back(state);
        }
        EXPECT_EQ(inst.ReadValidationErrorCount(), 0u) << "the settled tree must be conforming";
        inst.Shutdown();
        return out;
    }

    // Converges the motion tree at `pose` (every update with moving inputs), then holds the camera still
    // there for kStageUpdates rest updates.
    RestTrace MotionThenRest(const Pose& pose, const Shaping& shaping)
    {
        CBTInstance inst;
        EXPECT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
        EXPECT_TRUE(inst.InitializeRoots(kDomainPlanar));
        const ShapedRelief relief(*m_Device, inst, shaping);
        EXPECT_TRUE(relief.IsArmed()) << "the relief bound no height source, so the lattice is flat";
        CBTClassifyDesc c = ScreenClassify(shaping);
        CBTFrameParams p;
        BuildParams(p, pose, shaping);
        relief.Apply(p);
        c.Settle = 0u;
        RestTrace t;
        for (uint32_t i = 0; i < kStageUpdates; ++i)
        {
            RunUpdate(inst, c, p);
            t.KeepStepMax = std::max(t.KeepStepMax, inst.ReadWorkQueueCounter(kWQOffFrustumKeepStep));
            if (i < kStageUpdates / 2u)
                continue;
            const uint32_t live = inst.ReadTessellationStats().LiveCount;
            t.StillLiveMin = std::min(t.StillLiveMin, live);
            t.StillLiveMax = std::max(t.StillLiveMax, live);
        }
        t.MotionLive = inst.ReadTessellationStats().LiveCount;
        t.RestLiveMin = t.MotionLive;
        c.Settle = 1u;
        uint32_t quiet = 0;
        for (uint32_t i = 0; i < kStageUpdates; ++i)
        {
            RunUpdate(inst, c, p);
            const CBTTessellationStats s = inst.ReadTessellationStats();
            t.RestLiveMin = std::min(t.RestLiveMin, s.LiveCount);
            t.RestLiveEnd = s.LiveCount;
            t.KeepStepMax = std::max(t.KeepStepMax, s.OffFrustumKeepStep);
            if (i >= kStageUpdates / 2u)
            {
                t.RestStillMin = std::min(t.RestStillMin, s.LiveCount);
                t.RestStillMax = std::max(t.RestStillMax, s.LiveCount);
            }
            const bool changed = s.SplitServed != 0 || s.MergeServed != 0;
            quiet = changed ? 0u : quiet + 1u;
            t.RestUpdatesChanging = changed ? i + 1u : t.RestUpdatesChanging;
        }
        t.Quiet = quiet >= kQuietUpdates;
        EXPECT_EQ(inst.ReadValidationErrorCount(), 0u) << "the rest tree must be conforming";
        inst.Shutdown();
        return t;
    }

    // Holds the camera at `pose` for kStageUpdates updates under `shaping`, the first with Settle 0.
    StageRest HoldStage(CBTInstance& inst, const Pose& pose, const Shaping& shaping)
    {
        CBTClassifyDesc c = ScreenClassify(shaping);
        CBTFrameParams p;
        BuildParams(p, pose, shaping);
        StageRest out;
        uint32_t quietUpdates = 0;
        bool splitting = false;
        uint32_t splitRuns = 0;
        int32_t pressureStep = 0;
        int32_t keepStep = 0;
        for (uint32_t i = 0; i < kStageUpdates; ++i)
        {
            const bool moving = i == 0u;
            c.Settle = moving ? 0u : 1u;
            RunUpdate(inst, c, p);
            const CBTTessellationStats s = inst.ReadTessellationStats();
            splitRuns += (s.SplitServed != 0 && !splitting) ? 1u : 0u;
            splitting = s.SplitServed != 0;
            out.ArrivalPressureStep = moving ? s.PressureStep : out.ArrivalPressureStep;
            out.ArrivalMergeDemand = moving ? s.MergeDemand : out.ArrivalMergeDemand;
            out.ArrivalMergeCandidates = moving ? s.MergeCandidates : out.ArrivalMergeCandidates;
            out.RestMergeCandidatesMax = moving ? 0 : std::max(out.RestMergeCandidatesMax, s.MergeCandidates);
            out.RestPressureClimbs += (!moving && s.PressureStep > pressureStep) ? 1u : 0u;
            out.DownStepsAbovePressureZero += (!moving && s.OffFrustumKeepStep < keepStep && pressureStep > 0) ? 1u : 0u;
            pressureStep = s.PressureStep;
            keepStep = s.OffFrustumKeepStep;
            const bool changed = s.SplitServed != 0 || s.MergeServed != 0;
            quietUpdates = changed ? 0u : quietUpdates + 1u;
            out.UpdatesChanging = changed ? i + 1u : out.UpdatesChanging;
        }
        const CBTTessellationStats s = inst.ReadTessellationStats();
        out.Live = s.LiveCount;
        out.KeepStep = s.OffFrustumKeepStep;
        out.SplitRuns = splitRuns;
        out.Quiet = quietUpdates >= kQuietUpdates;
        return out;
    }

    // Rests at `pose`, turns the camera 180 degrees and turns back, each stage held for kStageUpdates.
    PathStages WalkPath(const Pose& pose, const Shaping& shaping)
    {
        CBTInstance inst;
        EXPECT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
        EXPECT_TRUE(inst.InitializeRoots(kDomainPlanar));
        const ShapedRelief relief(*m_Device, inst, shaping);
        EXPECT_TRUE(relief.IsArmed()) << "the relief bound no height source, so the lattice is flat";
        PathStages out;
        out.Rest = HoldStage(inst, pose, shaping);
        out.Turned = HoldStage(inst, Turned(pose), shaping);
        out.Back = HoldStage(inst, pose, shaping);
        EXPECT_EQ(inst.ReadValidationErrorCount(), 0u) << "the tree must be conforming";
        inst.Shutdown();
        return out;
    }

    // Converges the motion tree at `pose`, then moves the eye kJiggleM to one side and back on alternate
    // updates, every update with moving inputs (Settle 0). Returns the splits and merges served over the
    // last kJiggleCounted updates.
    uint64_t JiggleServed(const Pose& pose, const Shaping& shaping)
    {
        CBTInstance inst;
        EXPECT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
        EXPECT_TRUE(inst.InitializeRoots(kDomainPlanar));
        const ShapedRelief relief(*m_Device, inst, shaping);
        EXPECT_TRUE(relief.IsArmed()) << "the relief bound no height source, so the lattice is flat";
        CBTClassifyDesc c = ScreenClassify(shaping);
        CBTFrameParams here;
        CBTFrameParams there;
        Pose moved = pose;
        moved.EyeX += kJiggleM;
        BuildParams(here, pose, shaping);
        BuildParams(there, moved, shaping);
        relief.Apply(here);
        relief.Apply(there);
        c.Settle = 0u;
        for (uint32_t i = 0; i < kStageUpdates; ++i)
            RunUpdate(inst, c, here);
        uint64_t served = 0;
        for (uint32_t i = 0; i < kJiggleUpdates; ++i)
        {
            RunUpdate(inst, c, (i & 1u) == 0u ? there : here);
            const CBTTessellationStats s = inst.ReadTessellationStats();
            if (i + kJiggleCounted >= kJiggleUpdates)
                served += static_cast<uint64_t>(s.SplitServed) + static_cast<uint64_t>(s.MergeServed);
        }
        EXPECT_EQ(inst.ReadValidationErrorCount(), 0u) << "the tree must be conforming";
        inst.Shutdown();
        return served;
    }

    std::unique_ptr<IDevice> m_Device;
    CBTKernelSet m_KernelSet;
    uint32_t m_Frame = 0;
};

// #2702: one pose, two arrivals, one tree.
TEST_F(CBTRestSettle, ArrivalsFromNearAndFarSettleToOneTree)
{
    for (const Arrival& a : kArrivals)
    {
        SCOPED_TRACE(a.Label);
        const Settled fromNear = Arrive(a.FromNear, a.Rest);
        const Settled fromFar = Arrive(a.FromFar, a.Rest);
        std::printf("[rest:%s] live from near %u, from far %u; keep step %d / %d; merge demand %d / %d\n",
                    a.Label, fromNear.Stats.LiveCount, fromFar.Stats.LiveCount,
                    fromNear.Stats.OffFrustumKeepStep, fromFar.Stats.OffFrustumKeepStep,
                    fromNear.Stats.MergeDemand, fromFar.Stats.MergeDemand);

        // The controllers stay out of the comparison: no keep step and no contention at rest.
        for (const Settled* s : {&fromNear, &fromFar})
        {
            EXPECT_EQ(s->Stats.OffFrustumKeepStep, 0);
            EXPECT_EQ(s->Stats.PressureStep, 0);
            EXPECT_LT(static_cast<float>(s->Stats.LiveCount) / static_cast<float>(kDefaultBisectorPoolSize),
                      kContentionFreeOccupancy);
            EXPECT_TRUE(s->Quiet) << "the tree must stop changing at rest";
        }

        ASSERT_GT(fromNear.HeapIds.size(), 10000u) << "the rest pose barely refined; the test would prove nothing";
        EXPECT_EQ(fromNear.HeapIds.size(), fromFar.HeapIds.size());
        size_t onlyNear = 0, onlyFar = 0;
        for (size_t i = 0, j = 0; i < fromNear.HeapIds.size() || j < fromFar.HeapIds.size();)
        {
            const bool nearLeft = i < fromNear.HeapIds.size();
            if (j == fromFar.HeapIds.size() || (nearLeft && fromNear.HeapIds[i] < fromFar.HeapIds[j]))
                ++onlyNear, ++i;
            else if (i == fromNear.HeapIds.size() || fromFar.HeapIds[j] < fromNear.HeapIds[i])
                ++onlyFar, ++j;
            else
                ++i, ++j;
        }
        EXPECT_EQ(onlyNear, 0u) << "leaves only the arrival from near kept";
        EXPECT_EQ(onlyFar, 0u) << "leaves only the arrival from far kept";
    }
}

// #2600: merge demand at rest that is never served. The two children of a pair must agree on the merge
// at rest, or PrepareSimplify can never serve it. Both children ask the same question of the same
// parent box and the same twin box, so they reach one answer on any relief.
TEST_F(CBTRestSettle, PairsAgreeOnTheMergeAtRest)
{
    for (const Arrival& a : kArrivals)
    {
        SCOPED_TRACE(a.Label);
        const Settled rest = Arrive(a.FromNear, a.Rest);
        EXPECT_TRUE(rest.Quiet);
        uint64_t pairs = 0, disagree = 0, bothMerge = 0;
        for (size_t i = 0; i + 1 < rest.HeapIds.size(); ++i)
        {
            if ((rest.HeapIds[i] >> 1) != (rest.HeapIds[i + 1] >> 1))
                continue;
            ++pairs;
            const bool m0 = rest.States[i] == kStateSimplify, m1 = rest.States[i + 1] == kStateSimplify;
            disagree += m0 != m1 ? 1u : 0u;
            bothMerge += (m0 && m1) ? 1u : 0u;
        }
        std::printf("[agree:%s] sibling pairs %llu, disagreeing %llu, both asking to merge %llu, merge demand %d\n",
                    a.Label, static_cast<unsigned long long>(pairs), static_cast<unsigned long long>(disagree),
                    static_cast<unsigned long long>(bothMerge), rest.Stats.MergeDemand);
        ASSERT_GT(pairs, 1000u);
        EXPECT_EQ(disagree, 0u) << "a pair whose children disagree can never merge";
    }
}

// #2600, the half diamond: a merge flag that stands at rest although its diamond could merge. The four
// children of a diamond ask the same two parents (the facet's parent and its twin) the same question,
// so once the tree is quiet every flag that stands belongs to a diamond conformity holds: a finer
// neighbor splits a child of the twin, and PrepareSimplify waits for it.
TEST_F(CBTRestSettle, NoMergeFlagStandsOnADiamondThatCanMerge)
{
    for (const Arrival& a : kArrivals)
    {
        SCOPED_TRACE(a.Label);
        CBTTreeSnapshot snapshot;
        const Settled rest = Arrive(a.FromNear, a.Rest, &snapshot);
        ASSERT_TRUE(rest.Quiet);
        const auto leaves = LeafKeys(snapshot);
        uint32_t flags = 0;
        uint32_t mergeable = 0;
        for (uint32_t slot = 0; slot < snapshot.PoolSize; ++slot)
        {
            const uint64_t h = snapshot.HeapIds[slot];
            if (h == 0u || snapshot.Bisectors[slot].BisectorState != kStateSimplify)
                continue;
            ++flags;
            mergeable += DiamondChildrenAreLeaves(leaves, DiamondOf(LeafCorners(snapshot.Vertices[slot]), (h & 1u) != 0u))
                             ? 1u
                             : 0u;
        }
        std::printf("[standing:%s] live %zu, merge flags at rest %u, on a diamond of four leaves %u\n", a.Label,
                    leaves.size(), flags, mergeable);
        EXPECT_EQ(mergeable, 0u) << "a merge flag stands on a diamond that could merge";
    }
}

// The rest rule has no hysteresis band, so if a still camera's thresholds followed the live count the
// rest itself changes, a facet on the near-bias contention's threshold would split and merge on
// alternate updates forever. Kernel_Reset latches the live count on every update with moving inputs; at
// rest the latch follows only a move larger than 1/kRestLatchDeadbandDivisor of the pool, which a camera
// jump's rebuild makes and a rest's own flips do not. The depth-target classify sets the live count
// exactly: 2^depth facets.
TEST_F(CBTRestSettle, TheRestLatchFollowsOnlyLargeMovesAtRest)
{
    CBTInstance inst;
    ASSERT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
    ASSERT_TRUE(inst.InitializeRoots(kDomainPlanar));
    const CBTFrameParams p = CBTIdentityFrameParams();
    const uint32_t deadband = kDefaultBisectorPoolSize / kRestLatchDeadbandDivisor;
    auto run = [&](uint32_t targetDepth, uint32_t settle) {
        CBTClassifyDesc c{kClassifyDepthTarget, kFocusRootAll, targetDepth};
        c.Settle = settle;
        for (uint32_t i = 0; i < 24u; ++i)
            RunUpdate(inst, c, p);
        return inst.ReadTessellationStats().LiveCount;
    };
    auto latched = [&] { return static_cast<uint32_t>(inst.ReadWorkQueueCounter(kWQRestLiveCount)); };

    const uint32_t moving = run(8u, 0u);
    ASSERT_EQ(moving, 256u);
    EXPECT_EQ(latched(), moving) << "an update with moving inputs latches the live count";

    const uint32_t small = run(9u, 1u);
    ASSERT_EQ(small, 512u);
    ASSERT_LT(small - moving, deadband);
    EXPECT_EQ(latched(), moving) << "at rest a move inside the deadband leaves the latch";

    const uint32_t large = run(16u, 1u);
    ASSERT_EQ(large, 65536u);
    EXPECT_NE(latched(), moving) << "at rest a move past the deadband moves the latch";
    EXPECT_LE(large - latched(), deadband) << "the latch ends within the deadband of the tree it follows";
    inst.Shutdown();
}

// A pose whose tree fills the pool: at rest the splits the pool cannot serve roll back, and nothing may
// take the tree apart to make room, or the live count falls, the tree grows back and the rest never
// ends. Nothing reads occupancy here: the near-bias, the keep step and the pool-pressure scale are off.
TEST_F(CBTRestSettle, AFullPoolKeepsItsTreeAtRest)
{
    Shaping gateOnly;
    gateOnly.SplitPx = kFullPoolSplitPx;
    gateOnly.NearBias = false;
    gateOnly.KeepOcc = 0.0f;
    gateOnly.PoolPressure = 0u;
    const RestTrace t = MotionThenRest(kArrivals[1].Rest, gateOnly);
    std::printf("[fullpool] motion live %u (occupancy %.3f), rest minimum %u, rest end %u, quiet %d\n", t.MotionLive,
                static_cast<float>(t.MotionLive) / static_cast<float>(kDefaultBisectorPoolSize), t.RestLiveMin,
                t.RestLiveEnd, t.Quiet ? 1 : 0);
    ASSERT_GE(static_cast<float>(t.RestLiveEnd) / static_cast<float>(kDefaultBisectorPoolSize), 0.90f)
        << "the rest tree must fill the pool for the test to say anything";
    EXPECT_TRUE(t.Quiet) << "the tree must stop changing at rest";
    // A tree taken apart would lose far more than 5% of its facets.
    EXPECT_GE(static_cast<float>(t.RestLiveMin), 0.95f * static_cast<float>(t.MotionLive))
        << "the rest must not take the tree apart";
}

// A pool on the near-bias contention ramp, where the contention moves with the live count: the walk
// pose's rest tree sits past the ramp's foot and under its top. At rest the planar classify reads the
// live count latched at the stop
// (CBT_WQ_REST_LIVE_COUNT), so the contention's thresholds hold still and the tree goes quiet; read live,
// a facet on a threshold the tree's own size sets splits and merges on alternate updates forever.
TEST_F(CBTRestSettle, AContendedPoolGoesQuietAtRest)
{
    Shaping contended;
    contended.SplitPx = kContendedSplitPx;
    contended.KeepOcc = 0.0f;
    contended.PoolPressure = 0u;
    const RestTrace t = MotionThenRest(kArrivals[1].Rest, contended);
    const float occupancy = static_cast<float>(t.RestLiveEnd) / static_cast<float>(kDefaultBisectorPoolSize);
    std::printf("[contended] motion live %u, rest minimum %u, rest end %u (occupancy %.3f), quiet %d\n",
                t.MotionLive, t.RestLiveMin, t.RestLiveEnd, occupancy, t.Quiet ? 1 : 0);
    ASSERT_GT(occupancy, kContentionFreeOccupancy) << "the rest tree must sit on the contention ramp";
    ASSERT_LT(occupancy, 0.90f) << "the rest tree must sit under the contention ramp's top";
    EXPECT_TRUE(t.Quiet) << "the tree must stop changing at rest";
}

// #5355: with half the field behind the eye, the keep step holds a full pool still in both stages. If that
// field leaves in one step, occupancy falls under the hold band and the field refills the pool on every cycle.
// The first pose cycles under the motion threshold on a shared excess, the second under the rest threshold.
TEST_F(CBTRestSettle, APoolUnderTheKeepStepGoesQuietAtRest)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    for (const Pose& pose : {Above(256.0f, 2.0f, 256.0f, 0.0f, -4.0f), Above(256.0f, 2.0f, 300.0f, 45.0f, -4.0f)})
    {
        SCOPED_TRACE(pose.YawDeg);
        const RestTrace t = MotionThenRest(pose, pressured);
        std::printf("[keep-step] live over the stages' second halves %u to %u and %u to %u, keep step max %d, quiet %d\n",
                    t.StillLiveMin, t.StillLiveMax, t.RestStillMin, t.RestStillMax, t.KeepStepMax, t.Quiet ? 1 : 0);
        ASSERT_GT(t.KeepStepMax, 0) << "the pool must come under the keep step for the test to say anything";
        EXPECT_TRUE(t.Quiet) << "the tree must stop changing at rest";
        EXPECT_GE(static_cast<float>(t.StillLiveMin), kStillLiveBand * static_cast<float>(t.StillLiveMax))
            << "the live count must hold still under a still camera, not refill and drain";
        EXPECT_GE(static_cast<float>(t.RestStillMin), kStillLiveBand * static_cast<float>(t.RestStillMax))
            << "the live count must hold still at rest, not refill and drain";
    }
}

// #5355: under the keep step the tree at rest is a function of the pose. The camera rests, turns around and back;
// the turned rest matches a fresh arrival there and the return matches the first rest, in keep step and live count.
// Several keep steps fit the hold band, so a step that rests wherever the path left it fails here.
TEST_F(CBTRestSettle, AKeepStepTreeDependsOnThePoseNotThePath)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    for (const Pose& pose : kKeepPathPoses)
    {
        SCOPED_TRACE(pose.YawDeg);
        const PathStages path = WalkPath(pose, pressured);
        const StageRest& rest = path.Rest;
        const StageRest& turn = path.Turned;
        const StageRest& back = path.Back;
        CBTInstance fresh;
        ASSERT_TRUE(fresh.Initialize(*m_Device, m_KernelSet));
        ASSERT_TRUE(fresh.InitializeRoots(kDomainPlanar));
        const ShapedRelief freshRelief(*m_Device, fresh, pressured);
        ASSERT_TRUE(freshRelief.IsArmed());
        const StageRest direct = HoldStage(fresh, Turned(pose), pressured);
        fresh.Shutdown();
        std::printf("[keep-path] rest %u (keep step %d, %u updates), turned %u (%d, %u), back %u (%d, %u), "
                    "turned pose direct %u (%d, %u)\n",
                    rest.Live, rest.KeepStep, rest.UpdatesChanging, turn.Live, turn.KeepStep, turn.UpdatesChanging,
                    back.Live, back.KeepStep, back.UpdatesChanging, direct.Live, direct.KeepStep,
                    direct.UpdatesChanging);
        ASSERT_GT(rest.KeepStep, 0) << "the pool must come under the keep step for the test to say anything";
        for (const StageRest* stage : {&rest, &turn, &back, &direct})
        {
            EXPECT_TRUE(stage->Quiet) << "every stage must stop changing at rest";
            EXPECT_LE(stage->UpdatesChanging, kKeepSettleUpdates) << "every stage must settle within the bound";
        }
        EXPECT_EQ(back.KeepStep, rest.KeepStep) << "the return must rest at the keep step the first rest found";
        EXPECT_GE(static_cast<float>(std::min(rest.Live, back.Live)),
                  kStillLiveBand * static_cast<float>(std::max(rest.Live, back.Live)))
            << "the second rest at the pose must keep the tree the first rest kept, not what the turn left";
        EXPECT_EQ(turn.KeepStep, direct.KeepStep) << "the turned pose must rest at its own keep step";
        EXPECT_GE(static_cast<float>(std::min(turn.Live, direct.Live)),
                  kStillLiveBand * static_cast<float>(std::max(turn.Live, direct.Live)))
            << "the turned pose must keep the tree a fresh arrival there keeps";
    }
}

// #5421, #5416: a stop under the keep step costs one settle. The rest starts one keep step down, so the probe
// of that step refines while the old view merges, in the same split run.
TEST_F(CBTRestSettle, AStopSettlesInOneSplitRun)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    uint32_t stopUpdates = 0;
    for (uint32_t i = 0; i < 2u; ++i)
    {
        SCOPED_TRACE(i);
        const PathStages path = WalkPath(kKeepPathPoses[i], pressured);
        std::printf("[stop] pose %u: turned %u updates, %u split runs; back %u updates, %u split runs\n", i + 1u,
                    path.Turned.UpdatesChanging, path.Turned.SplitRuns, path.Back.UpdatesChanging, path.Back.SplitRuns);
        ASSERT_GT(path.Rest.KeepStep, 0) << "the pool must come under the keep step for the test to say anything";
        EXPECT_TRUE(path.Turned.Quiet && path.Back.Quiet) << "every stage must stop changing at rest";
        stopUpdates += path.Turned.UpdatesChanging + path.Back.UpdatesChanging;
        if (i != 0u)
            continue;
        EXPECT_EQ(path.Turned.SplitRuns, 1u) << "the probe must refine in the stop's own split run";
        EXPECT_EQ(path.Back.SplitRuns, 1u) << "the probe must refine in the stop's own split run";
    }
    EXPECT_LE(stopUpdates, kStopSettleUpdates) << "the stops must settle within the bound";
}

// #5563: at rest under keep step 24 the pressure step does not climb. The rest tree settles under the keep
// ceiling, where the pressure step recovers to 0, so a climb during the keep search only coarsens the view
// and hides the merge-free update the keep step's up rule reads.
TEST_F(CBTRestSettle, ThePressureStepYieldsToTheKeepStepAtRest)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    for (const Pose& pose : kKeepPathPoses)
    {
        SCOPED_TRACE(pose.YawDeg);
        const PathStages path = WalkPath(pose, pressured);
        for (const StageRest* stage : {&path.Rest, &path.Turned, &path.Back})
        {
            ASSERT_GT(stage->KeepStep, 0) << "the pool must come under the keep step for the test to say anything";
            ASSERT_LT(stage->KeepStep, int32_t(kOffFrustumKeepSteps)) << "the keep step must be able to rise";
            EXPECT_EQ(stage->RestPressureClimbs, 0u) << "the pressure step climbed during the keep search";
            EXPECT_TRUE(stage->Quiet) << "every stage must stop changing at rest";
        }
    }
}

// The early up proof reads the merge candidates of a rest update. A moving update counts none, so the camera in
// motion pays no atomic per merging leaf.
TEST_F(CBTRestSettle, MergeCandidatesAreCountedOnlyAtRest)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    const PathStages path = WalkPath(kKeepPathPoses[0], pressured);
    const StageRest& turned = path.Turned;
    ASSERT_GT(turned.ArrivalMergeDemand, 0) << "the turn must leave leaves to merge for the test to say anything";
    EXPECT_EQ(turned.ArrivalMergeCandidates, 0) << "a moving update counted merge candidates";
    EXPECT_GT(turned.RestMergeCandidatesMax, 0) << "a rest update with merges to make counted no merge candidates";
}

// A rest steps the keep step down only at pressure step 0: a tree measured above it is smaller than the settled tree.
// The arrival fills the pool in motion until the pressure step climbs, then a coarser target drains the pool under
// the keep ceiling in motion, so the rest starts under the ceiling with the pressure step raised.
TEST_F(CBTRestSettle, AKeepStepStepsDownOnlyAtPressureStepZero)
{
    Shaping pressured;
    pressured.SplitPx = kFullPoolSplitPx;
    Shaping coarser;
    coarser.SplitPx = kContendedSplitPx;
    const Pose& pose = kKeepPathPoses[0];
    CBTInstance inst;
    ASSERT_TRUE(inst.Initialize(*m_Device, m_KernelSet));
    ASSERT_TRUE(inst.InitializeRoots(kDomainPlanar));
    const ShapedRelief relief(*m_Device, inst, pressured);
    ASSERT_TRUE(relief.IsArmed());

    CBTClassifyDesc filling = ScreenClassify(pressured);
    filling.Settle = 0u;
    CBTFrameParams fillingParams;
    BuildParams(fillingParams, pose, pressured);
    CBTTessellationStats stats = inst.ReadTessellationStats();
    for (uint32_t i = 0; i < kStageUpdates && stats.PressureStep < kArrivalPressureStep; ++i)
    {
        RunUpdate(inst, filling, fillingParams);
        stats = inst.ReadTessellationStats();
    }
    ASSERT_GE(stats.PressureStep, kArrivalPressureStep) << "the arrival must climb the pressure step in motion";

    CBTClassifyDesc draining = ScreenClassify(coarser);
    draining.Settle = 0u;
    CBTFrameParams drainingParams;
    BuildParams(drainingParams, pose, coarser);
    for (uint32_t i = 0; i < kStageUpdates && stats.LiveCount >= uint32_t(kOffFrustumKeepOcc * float(stats.PoolSize));
         ++i)
    {
        RunUpdate(inst, draining, drainingParams);
        stats = inst.ReadTessellationStats();
    }
    ASSERT_GT(stats.OffFrustumKeepStep, 0) << "the keep step must have a step to take down";

    const StageRest rest = HoldStage(inst, pose, coarser);
    EXPECT_EQ(inst.ReadValidationErrorCount(), 0u) << "the tree must be conforming";
    inst.Shutdown();
    ASSERT_GT(rest.ArrivalPressureStep, 0) << "the rest must start above pressure step 0 for the test to say anything";
    EXPECT_EQ(rest.DownStepsAbovePressureZero, 0u) << "the keep step stepped down above pressure step 0";
}

// #4823: the screen-space edge rule (here the atlas source) comes to rest at a still camera within a bounded
// number of updates. Its near-field merge measures the child's own edge, so one threshold for the split and
// the merge would flip the facets that straddle it.
TEST_F(CBTRestSettle, TheEdgeRuleSettlesAtRest)
{
    Shaping edge;
    edge.EdgeRule = true;
    for (const Arrival& a : kArrivals)
    {
        SCOPED_TRACE(a.Label);
        const RestTrace t = MotionThenRest(a.Rest, edge);
        std::printf("[edge-rest:%s] motion live %u, rest minimum %u, rest end %u, rest updates changing %u, quiet %d\n",
                    a.Label, t.MotionLive, t.RestLiveMin, t.RestLiveEnd, t.RestUpdatesChanging, t.Quiet ? 1 : 0);
        ASSERT_GT(t.MotionLive, 10000u) << "the pose barely refined; the test would prove nothing";
        EXPECT_TRUE(t.Quiet) << "the tree must stop changing at rest";
        EXPECT_LE(t.RestUpdatesChanging, kEdgeRuleRestUpdates) << "the rest must settle within a bound";
    }
}

// The edge rule's motion band: while the camera moves, a facet splits well above the threshold its
// children merge below, so a camera that moves a centimeter and back does not split and merge the same
// facets on every update. The walk pose holds the near field under the eye, where the split and the merge
// measure different edges. (At the overview a centimeter moves a few facets across the eye plane, from the
// near-field rule to the screen-space rule and back, which splits and merges them whatever the band.)
TEST_F(CBTRestSettle, TheEdgeRuleDoesNotChurnUnderASmallMove)
{
    Shaping edge;
    edge.EdgeRule = true;
    const Arrival& walk = kArrivals[1];
    const uint64_t served = JiggleServed(walk.Rest, edge);
    std::printf("[edge-jiggle:%s] splits and merges served over the last %u updates: %llu\n", walk.Label,
                kJiggleCounted, static_cast<unsigned long long>(served));
    EXPECT_EQ(served, 0u) << "a centimeter of camera motion must not churn the mesh";
}
