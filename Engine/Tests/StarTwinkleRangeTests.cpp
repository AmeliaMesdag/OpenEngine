// Contract for each night-sky star's twinkle range (star_sparkle.glsl, GE_StarTwinkleRange).
//
// The twinkle dims the whole star at its low phase, and stars differ in how bright they peak and
// how deep they dip, so the sky is not every star at pure white. The range comes from a hash of
// the star's index in the shipped GLSL, so the distribution is evaluated on the device
// (star_twinkle_probe.comp) and this file reads it back.
//
// Skips ONLY without a Vulkan device. A missing staged probe FAILS: the same target builds it.

#include <gtest/gtest.h>

#include "Rendering/Common/Utils.h"
#include "Rendering/Core/CommandList.h"
#include "Rendering/Core/Device.h"
#include "Rendering/Core/PipelineTypes.h"

#include "Platform/Shell.h"

#include "TestDeviceHelper.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

using namespace GameEngine;
using namespace GameEngine::Rendering;

namespace
{

// Mirrors star_twinkle_probe.comp's ProbePC.
struct ProbePC
{
    float counts[4]; // star count, unused x3
};

constexpr uint32_t kStars = 4096u;

// One star's range as the device computed it.
struct StarTwinkle
{
    float Peak = 0.0f;
    float Low = 0.0f;
    float AtLowPhase = 0.0f;
    float AtPeak = 0.0f;
};

std::filesystem::path FindProbeSpv()
{
    namespace fs = std::filesystem;
    const fs::path exe = GameEngine::Platform::GetExecutablePath();
    fs::path dir = exe.empty() ? fs::current_path() : exe.parent_path();
    for (int up = 0; up < 6; ++up)
    {
        const fs::path candidate = dir / "Shaders" / "star_twinkle_probe.comp.spv";
        std::error_code ec;
        if (fs::exists(candidate, ec))
            return candidate;
        if (!dir.has_parent_path() || dir.parent_path() == dir)
            break;
        dir = dir.parent_path();
    }
    return {};
}

class StarTwinkleRangeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_Device = CreateVulkanDeviceFast();
        if (!m_Device)
            GTEST_SKIP() << "No Vulkan device available";

        const std::filesystem::path spvPath = FindProbeSpv();
        ASSERT_FALSE(spvPath.empty())
            << "star_twinkle_probe.comp.spv was not found above the test executable ("
            << GameEngine::Platform::GetExecutablePath().string()
            << ") -- the shader target stages it, so its absence is a build defect";
        const std::vector<uint8_t> spv = Utils::ReadFile(spvPath.string());
        ASSERT_FALSE(spv.empty()) << "staged probe SPIR-V is empty: " << spvPath.string();

        DescriptorBinding out{};
        out.binding = 0;
        out.type = DescriptorType::StorageBuffer;
        out.count = 1;
        out.shaderStages = kShaderStageCompute;
        m_Layout.debugName = "StarTwinkleProbe.Set0";
        m_Layout.bindings = {out};

        ComputePipelineDesc cd{};
        cd.ComputeShader = std::make_shared<const std::vector<uint8_t>>(spv);
        cd.DescriptorSetLayouts.push_back(m_Device->InternDescriptorSetLayout(m_Layout));
        cd.PushConstants.Size = sizeof(ProbePC);
        cd.PushConstants.StageMask = kShaderStageCompute;
        cd.DebugName = "StarTwinkleProbe";
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

    std::vector<StarTwinkle> Probe()
    {
        ProbePC pc{};
        pc.counts[0] = static_cast<float>(kStars);
        const size_t bytes = static_cast<size_t>(kStars) * 4u * sizeof(float);
        BufferDesc od{};
        od.size = bytes;
        od.usage = static_cast<uint32_t>(BufferUsage::Storage) |
                   static_cast<uint32_t>(BufferUsage::TransferSrc) |
                   static_cast<uint32_t>(BufferUsage::TransferDst);
        od.memoryUsage = BufferMemoryUsage::Readback;
        od.flags = BufferCreateFlags::PersistentlyMapped;
        od.debugName = "StarTwinkleProbe.Out";
        m_OutBuffer = m_Device->CreateBuffer(od);
        // Poison: a dispatch that never lands is distinguishable from one that wrote zeros.
        const std::vector<float> poison(static_cast<size_t>(kStars) * 4u, -1.0f);
        m_Device->UpdateBuffer(m_OutBuffer, 0, bytes, poison.data());

        DescriptorSetDesc dsDesc{};
        dsDesc.layout = m_Layout;
        dsDesc.transient = true;
        dsDesc.debugName = "StarTwinkleProbe.DS";
        DescriptorSetHandle ds = m_Device->CreateDescriptorSet(dsDesc);
        m_Device->UpdateStorageBufferBinding(ds, 0, m_OutBuffer, 0, bytes);

        auto cl = m_Device->CreateCommandList(IDevice::QueueType::Compute);
        cl->Begin();
        cl->SetPipeline(m_Pipeline);
        cl->BindDescriptorSet(0, ds, m_Pipeline);
        cl->SetPushConstants(pc);
        cl->Dispatch((kStars + 63u) / 64u, 1u, 1u);
        cl->End();
        m_Device->ExecuteCommandLists({cl.get()});
        m_Device->WaitForIdle();

        std::vector<float> raw(static_cast<size_t>(kStars) * 4u);
        void* mapped = m_Device->MapBuffer(m_OutBuffer);
        EXPECT_NE(mapped, nullptr);
        if (mapped)
            std::memcpy(raw.data(), mapped, bytes);
        m_Device->UnmapBuffer(m_OutBuffer);

        std::vector<StarTwinkle> stars(kStars);
        for (uint32_t i = 0; i < kStars; ++i)
            stars[i] = {raw[i * 4u + 0u], raw[i * 4u + 1u], raw[i * 4u + 2u], raw[i * 4u + 3u]};
        return stars;
    }

    std::unique_ptr<IDevice> m_Device;
    DescriptorSetLayoutDesc m_Layout{};
    PipelineHandle m_Pipeline{};
    BufferHandle m_OutBuffer{};
};

} // namespace

// The distribution the sky is tuned to: one star in five faint and barely twinkling (peak 0.2,
// low phase 0.9 of it); the rest peak at 0.2 + 0.8 h^4 of full, so half of them at 0.25 or less
// and about one in thirty at 0.9 or more, and dip to 0.4 to 0.7 of their own peak. A constant
// core (every star at full, never dipping) fails here, and so does the earlier range, 0.6 to 1.0,
// whose peaks all sat on the tone curve's shoulder.
TEST_F(StarTwinkleRangeTest, StarsPeakAndDipEachByTheirOwnRange)
{
    const std::vector<StarTwinkle> stars = Probe();
    ASSERT_GE(stars.front().Peak, 0.0f) << "the probe never wrote its first slot: the dispatch did not land";

    std::vector<float> peaks;
    uint32_t faint = 0;
    uint32_t nearWhite = 0;
    float lowestDip = 1.0f;
    float deepestTwinklerDip = 0.0f;
    for (const StarTwinkle& star : stars)
    {
        EXPECT_GE(star.Peak, 0.2f);
        EXPECT_LE(star.Peak, 1.0f);
        peaks.push_back(star.Peak);
        if (star.Peak == 0.2f && star.Low == 0.9f)
        {
            ++faint;
            continue;
        }
        EXPECT_GE(star.Low, 0.4f);
        EXPECT_LE(star.Low, 0.7f);
        lowestDip = std::min(lowestDip, star.Low);
        deepestTwinklerDip = std::max(deepestTwinklerDip, star.Low);
        if (star.Peak >= 0.9f)
            ++nearWhite;
    }
    const float faintShare = static_cast<float>(faint) / kStars;
    const float nearWhiteShare = static_cast<float>(nearWhite) / kStars;
    EXPECT_NEAR(faintShare, 0.2f, 0.03f) << "one star in five stays faint and barely twinkles";
    EXPECT_NEAR(nearWhiteShare, 0.026f, 0.01f) << "about one star in forty peaks at 0.9 of full or more";
    std::nth_element(peaks.begin(), peaks.begin() + peaks.size() / 2, peaks.end());
    EXPECT_LE(peaks[peaks.size() / 2], 0.3f) << "the median star's peak sits on the tone curve's shoulder";
    EXPECT_LT(lowestDip, 0.42f) << "the dips do not reach down to 0.4 of a star's peak";
    EXPECT_GT(deepestTwinklerDip, 0.68f) << "the dips do not span up to 0.7 of a star's peak";
}

// The low phase dims the whole star: its brightness there is its peak times its dip, and its
// brightness at the peak is its peak.
TEST_F(StarTwinkleRangeTest, TheLowPhaseIsTheStarsPeakTimesItsDip)
{
    const std::vector<StarTwinkle> stars = Probe();
    for (uint32_t i = 0; i < kStars; ++i)
    {
        EXPECT_FLOAT_EQ(stars[i].AtPeak, stars[i].Peak) << "star " << i;
        EXPECT_NEAR(stars[i].AtLowPhase, stars[i].Peak * stars[i].Low, 1e-6f) << "star " << i;
        EXPECT_LT(stars[i].AtLowPhase, stars[i].AtPeak) << "star " << i << " does not dim at its low phase";
    }
}
