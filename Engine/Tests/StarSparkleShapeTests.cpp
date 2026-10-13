// Contract for the night sky's star sparkle (star_sparkle.glsl).
//
// A star is a point source on the scene-linear scale, so the exposure decides how much of its
// sprite clips to white, and what reads on screen is the clipped region's outline: a level
// set of the sparkle. The sparkle must keep four points at every exposure, from a dim star to
// one an exposure drives a hundredfold past white. A convex metric fails this silently: its
// level sets are squares, and every star turns into a hard-edged rotated square once the
// exposure clips it, while a dim star still looks like a soft diamond.
//
// The sparkle is evaluated on the device (star_sparkle_probe.comp) and this file only reads
// the profiles back. The source contract pins that the stars and the falling stars' heads
// reach the sparkle through the shared include, so the probe measures what ships.
//
// Skips ONLY without a Vulkan device. A missing staged probe FAILS: the same target builds it.

#include <gtest/gtest.h>

#include "Rendering/Common/Utils.h"
#include "Rendering/Core/CommandList.h"
#include "Rendering/Core/Device.h"
#include "Rendering/Core/PipelineTypes.h"

#include "Platform/Shell.h"

#include "TestDeviceHelper.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace GameEngine;
using namespace GameEngine::Rendering;

namespace
{

// Mirrors star_sparkle_probe.comp's ProbePC.
struct ProbePC
{
    float shape[4];  // core radius, point amount, core size, falloff
    float counts[4]; // samples per ray, samples on the unit circle, unused, unused
};

// The stars' default shape, as SkyRenderNode uploads it for a default SkyEnvironment: a core
// of radius 0.225 / 0.5625 of the points' reach (kBaseStarRadius / kBaseDiamondRadius),
// StarDiamondShape 0.5, StarCoreSize 0.15, StarGlowFalloff 10.
constexpr float kCoreRadius = 0.4f;
constexpr float kPointAmount = 0.5f;
constexpr float kCoreSize = 0.15f;
constexpr float kFalloff = 10.0f;

constexpr uint32_t kRaySamples = 2048u;
constexpr uint32_t kCircleSamples = 256u;

// How far past the tonemapper's white point the star's centre sits, from a star whose points
// just clip to one whose round core is most of what clips.
constexpr float kExposureScales[] = {16.0f, 64.0f, 256.0f};
// A point reaches at least twice as far as the flank between two points. The four-pointed
// metric alone gives 2 sqrt(2); with the round core widening the flank it gives 2.18, 2.27 and
// 2.33 at the scales above. A square outline gives sqrt(2) at every scale.
constexpr double kMinPointToFlankReach = 2.0;

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f.is_open())
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Reads a repo shader source via GE_RENDERER_REPO_ROOT, the dev-only anchor the sky's other
// source contracts use: the source is the artifact under test.
std::string LoadShaderSource(const char* relativePath)
{
#ifndef GE_RENDERER_REPO_ROOT
    (void)relativePath;
    return {};
#else
    return ReadTextFile(std::filesystem::path(GE_RENDERER_REPO_ROOT) / "Engine/Modules/Rendering/Shaders" /
                        relativePath);
#endif
}

// Resolve the staged probe SPIR-V by walking up from the executable, never from the working
// directory, so a run from the binary's own directory still finds it.
std::filesystem::path FindProbeSpv()
{
    namespace fs = std::filesystem;
    const fs::path exe = GameEngine::Platform::GetExecutablePath();
    fs::path dir = exe.empty() ? fs::current_path() : exe.parent_path();
    for (int up = 0; up < 6; ++up)
    {
        const fs::path candidate = dir / "Shaders" / "star_sparkle_probe.comp.spv";
        std::error_code ec;
        if (fs::exists(candidate, ec))
            return candidate;
        if (!dir.has_parent_path() || dir.parent_path() == dir)
            break;
        dir = dir.parent_path();
    }
    return {};
}

// What one probe run returns: the sparkle along a point, along the diagonal between two
// points, and around the unit circle.
struct SparkleProfiles
{
    std::vector<float> Point;
    std::vector<float> Flank;
    std::vector<float> UnitCircle;
};

// Radius of the outline a clip at 1 / exposureScale leaves along one ray: the outermost
// sample still at or above it, or 0 when none is.
double ClippedReach(const std::vector<float>& profile, float exposureScale)
{
    double reach = 0.0;
    for (uint32_t k = 0; k < profile.size(); ++k)
        if (profile[k] * exposureScale >= 1.0f)
            reach = (static_cast<double>(k) + 0.5) / static_cast<double>(profile.size());
    return reach;
}

class StarSparkleShapeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_Device = CreateVulkanDeviceFast();
        if (!m_Device)
            GTEST_SKIP() << "No Vulkan device available";

        const std::filesystem::path spvPath = FindProbeSpv();
        ASSERT_FALSE(spvPath.empty())
            << "star_sparkle_probe.comp.spv was not found above the test executable ("
            << GameEngine::Platform::GetExecutablePath().string()
            << ") -- the shader target stages it, so its absence is a build defect";
        const std::vector<uint8_t> spv = Utils::ReadFile(spvPath.string());
        ASSERT_FALSE(spv.empty()) << "staged probe SPIR-V is empty: " << spvPath.string();

        DescriptorBinding out{};
        out.binding = 0;
        out.type = DescriptorType::StorageBuffer;
        out.count = 1;
        out.shaderStages = kShaderStageCompute;
        m_Layout.debugName = "StarSparkleProbe.Set0";
        m_Layout.bindings = {out};

        ComputePipelineDesc cd{};
        cd.ComputeShader = std::make_shared<const std::vector<uint8_t>>(spv);
        cd.DescriptorSetLayouts.push_back(m_Device->InternDescriptorSetLayout(m_Layout));
        cd.PushConstants.Size = sizeof(ProbePC);
        cd.PushConstants.StageMask = kShaderStageCompute;
        cd.DebugName = "StarSparkleProbe";
        m_Pipeline = m_Device->GetOrCreateComputePipeline(m_Device->InternComputePipeline(cd));
        ASSERT_TRUE(m_Pipeline.IsValid());
    }

    void TearDown() override
    {
        if (m_OutBuffer.IsValid())
            m_Device->DestroyBuffer(m_OutBuffer);
        if (m_Device)
            m_Device->Shutdown();
    }

    SparkleProfiles Probe()
    {
        ProbePC pc{};
        pc.shape[0] = kCoreRadius;
        pc.shape[1] = kPointAmount;
        pc.shape[2] = kCoreSize;
        pc.shape[3] = kFalloff;
        pc.counts[0] = static_cast<float>(kRaySamples);
        pc.counts[1] = static_cast<float>(kCircleSamples);

        const uint32_t slots = 2u * kRaySamples + kCircleSamples;
        const size_t bytes = static_cast<size_t>(slots) * 4u * sizeof(float);
        BufferDesc od{};
        od.size = bytes;
        od.usage = static_cast<uint32_t>(BufferUsage::Storage) |
                   static_cast<uint32_t>(BufferUsage::TransferSrc) |
                   static_cast<uint32_t>(BufferUsage::TransferDst);
        od.memoryUsage = BufferMemoryUsage::Readback;
        od.flags = BufferCreateFlags::PersistentlyMapped;
        od.debugName = "StarSparkleProbe.Out";
        m_OutBuffer = m_Device->CreateBuffer(od);
        // Poison: a dispatch that never lands is distinguishable from one that wrote zeros.
        const std::vector<float> poison(static_cast<size_t>(slots) * 4u, -1.0f);
        m_Device->UpdateBuffer(m_OutBuffer, 0, bytes, poison.data());

        DescriptorSetDesc dsDesc{};
        dsDesc.layout = m_Layout;
        dsDesc.transient = true;
        dsDesc.debugName = "StarSparkleProbe.DS";
        DescriptorSetHandle ds = m_Device->CreateDescriptorSet(dsDesc);
        m_Device->UpdateStorageBufferBinding(ds, 0, m_OutBuffer, 0, bytes);

        auto cl = m_Device->CreateCommandList(IDevice::QueueType::Compute);
        cl->Begin();
        cl->SetPipeline(m_Pipeline);
        cl->BindDescriptorSet(0, ds, m_Pipeline);
        cl->SetPushConstants(pc);
        cl->Dispatch((slots + 63u) / 64u, 1u, 1u);
        cl->End();
        m_Device->ExecuteCommandLists({cl.get()});
        m_Device->WaitForIdle();

        std::vector<float> raw(static_cast<size_t>(slots) * 4u);
        void* mapped = m_Device->MapBuffer(m_OutBuffer);
        EXPECT_NE(mapped, nullptr);
        if (mapped)
            std::memcpy(raw.data(), mapped, bytes);
        m_Device->UnmapBuffer(m_OutBuffer);

        SparkleProfiles p{};
        for (uint32_t i = 0; i < slots; ++i)
        {
            const float value = raw[i * 4u + 1u];
            if (i < kRaySamples)
                p.Point.push_back(value);
            else if (i < 2u * kRaySamples)
                p.Flank.push_back(value);
            else
                p.UnitCircle.push_back(value);
        }
        return p;
    }

    std::unique_ptr<IDevice> m_Device;
    DescriptorSetLayoutDesc m_Layout{};
    PipelineHandle m_Pipeline{};
    BufferHandle m_OutBuffer{};
};

} // namespace

// THE property. At every exposure that clips the star, the clipped outline keeps four points:
// along a point it reaches at least twice as far as along the diagonal between two points.
TEST_F(StarSparkleShapeTest, ClippedOutlineKeepsFourPointsAtEveryExposure)
{
    const SparkleProfiles p = Probe();
    ASSERT_EQ(p.Point.size(), kRaySamples);
    ASSERT_GE(p.Point.front(), 0.0f) << "the probe never wrote its first slot: the dispatch did not land";

    double previousPointReach = 0.0;
    for (const float scale : kExposureScales)
    {
        const double pointReach = ClippedReach(p.Point, scale);
        const double flankReach = ClippedReach(p.Flank, scale);
        ASSERT_GT(flankReach, 0.0) << "nothing clips at " << scale << "x: the scale tests no outline";
        EXPECT_GE(pointReach / flankReach, kMinPointToFlankReach)
            << "at " << scale << "x past white the clipped outline reaches " << pointReach
            << " along a point and " << flankReach << " between two points: the star clips into "
            << "a square, not a four-pointed star";
        EXPECT_GT(pointReach, previousPointReach)
            << "a brighter exposure must widen the outline, not hold it";
        previousPointReach = pointReach;
    }
}

// The sparkle is full at its centre and already zero on the unit circle, so a billboard whose
// quad holds the unit disc never cuts a lit sprite at its edge.
TEST_F(StarSparkleShapeTest, SparkleIsFullAtTheCentreAndZeroOnTheUnitCircle)
{
    const SparkleProfiles p = Probe();
    ASSERT_EQ(p.UnitCircle.size(), kCircleSamples);
    EXPECT_FLOAT_EQ(p.Point.front(), 1.0f) << "the core is not full at the star's centre";
    for (uint32_t k = 0; k < kCircleSamples; ++k)
        EXPECT_EQ(p.UnitCircle[k], 0.0f) << "the sparkle is lit on the unit circle at sample " << k;
}

// The stars and both falling-star heads draw the sparkle through the shared include, so the
// probe above measures the shape a camera sees (filtered over the pixel's footprint, which
// averages four taps of the same GE_StarSparkle).
TEST(StarSparkleSourceContract, StarsAndFallingStarHeadsDrawTheSharedSparkle)
{
    const std::string stars = LoadShaderSource("sky_stars_billboard.frag");
    const std::string sky = LoadShaderSource("sky_render.frag");
    ASSERT_FALSE(stars.empty()) << "sky_stars_billboard.frag was not readable (GE_RENDERER_REPO_ROOT)";
    ASSERT_FALSE(sky.empty()) << "sky_render.frag was not readable (GE_RENDERER_REPO_ROOT)";

    for (const std::string* source : {&stars, &sky})
        EXPECT_NE(source->find("#include \"Includes/star_sparkle.glsl\""), std::string::npos);
    EXPECT_NE(stars.find("GE_StarSparkleFiltered("), std::string::npos)
        << "the star billboard no longer draws the shared, pixel-filtered sparkle";
    EXPECT_NE(sky.find("GE_StarSparkleFiltered("), std::string::npos)
        << "the falling-star head no longer draws the shared, pixel-filtered sparkle";

    std::size_t headCalls = 0;
    for (std::size_t pos = sky.find("FallingStarHead("); pos != std::string::npos;
         pos = sky.find("FallingStarHead(", pos + 1))
        ++headCalls;
    EXPECT_EQ(headCalls, 3u) << "sky_render.frag must define FallingStarHead and call it from the "
                                "perspective and the 2D backdrop falling stars";
}
