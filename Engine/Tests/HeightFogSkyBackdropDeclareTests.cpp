// The height fog's sky colour reaches the fog pass through the render graph: SkyRender publishes
// the texture it draws its backdrop from as SkyBackdrop, and the shipped HeightFog pass reads it
// only while the fog takes its colour from the sky. Part of RenderPipelineDeclareTests.
#include "Engine/Rendering/Pipeline/Nodes/EngineNodeTypes.h"
#include "Engine/Rendering/Pipeline/RenderPipeline.h"
#include "Engine/Rendering/Pipeline/RenderPipelineNodes.h"
#include "Engine/Rendering/PostProcessSettings.h"
#include "Engine/Rendering/RenderServices.h"
#include "Engine/Rendering/SkyRenderFeature.h"
#include "Components/Rendering/PostProcessEffects/HeightFogEffect.h"
#include "Mathematics/Matrix4x4.h"
#include "Rendering/CameraTypes.h"
#include "Rendering/Core/Device.h"
#include "Rendering/Core/RenderGraph/RGFrame.h"
#include "Rendering/Sky/SkySettings.h"
#include "Rendering/Sky/SkySystem.h"
#include "TestDeviceHelper.h"
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace GameEngine;
using namespace GameEngine::Rendering;
using namespace GameEngine::Engine::Renderer;
using namespace GameEngine::Engine::Renderer::Pipeline;

namespace
{

constexpr const char* kTextureResource =
    R"({"kind":"texture","scope":"perView","format":"r16g16b16a16_float","extent":{"scale":[1,1]},"usage":["renderTarget","shaderResource"]})";

TextureDesc ViewTargetDesc(TextureFormat format, uint32_t usage)
{
    TextureDesc d{};
    d.width = 64;
    d.height = 64;
    d.depth = 1;
    d.mipLevels = 1;
    d.arrayLayers = 1;
    d.sampleCount = 1;
    d.format = static_cast<uint32_t>(format);
    d.usage = usage;
    return d;
}

// The shipped pipeline's own pass declarations, so the test reads the gate and the input as they ship.
nlohmann::json ShippedPass(const nlohmann::json& pipeline, const char* id)
{
    for (const nlohmann::json& pass : pipeline["passes"])
        if (pass.value("id", std::string()) == id)
            return pass;
    ADD_FAILURE() << "ForwardPlus.rendergraph has no pass '" << id << "'";
    return {};
}

RenderPipelineBlueprint::Pass Step(const nlohmann::json& pass)
{
    RenderPipelineBlueprint::Pass p;
    p.id = pass.value("id", std::string());
    p.type = pass.value("type", std::string());
    p.enabled = true;
    p.perView = true;
    p.passJson = pass.dump();
    return p;
}

struct SkyBackdropFixture
{
    std::unique_ptr<IDevice> Device;
    std::unique_ptr<RenderServices> Rs;
    RenderPipelineNodeRegistry Registry;
    Rendering::ViewId View{};

    bool Up(Components::HeightFogColorMode mode)
    {
        Device = CreateVulkanDeviceFast();
        if (!Device)
            return false;
        Rs = std::make_unique<RenderServices>();
        EXPECT_TRUE(Rs->Initialize(Device.get()));

        const Mathematics::Matrix4x4 view = Mathematics::Matrix4x4::LookAt(
            Mathematics::Vector3(0.0f, 3.0f, 0.0f), Mathematics::Vector3(0.0f, 3.0f, 1.0f),
            Mathematics::Vector3(0.0f, 1.0f, 0.0f));
        const Mathematics::Matrix4x4 projection =
            Mathematics::Matrix4x4::PerspectiveReverseZ(1.0471976f, 1.0f, 0.5f, 600.0f);
        const Mathematics::Matrix4x4 viewProj = projection * view;
        CameraData camera{};
        std::memcpy(camera.view, view.Data(), sizeof(camera.view));
        std::memcpy(camera.proj, projection.Data(), sizeof(camera.proj));
        std::memcpy(camera.viewProj, viewProj.Data(), sizeof(camera.viewProj));
        camera.cameraPos[1] = 3.0f;
        const CameraId cam = Rs->Views().AllocateCamera("SkyBackdropCam");
        Rs->Views().SetCameraData(cam, camera);
        View = Rs->Views().AllocateView("SkyBackdropView", cam);
        Rs->Views().SetViewRenderLayerMask(View, 1u);
        Rendering::ViewClearConfig clear{};
        clear.clearColor = true;
        clear.clearDepth = true;
        Rs->Views().SetViewTargets(View, 0, 0, 0, clear);

        PostProcessSettings pp{};
        pp.HeightFogIntensity = 1.0f;
        pp.HeightFogDensity = 0.5f;
        pp.HeightFogColorMode = static_cast<int32>(mode);
        Rs->Views().SetViewPostProcessOverride(View, pp);

        // The physical sky, active: what a scene with a Sky Environment hands the renderer.
        auto& sky = Rs->EnsureFeature<SkyRenderFeature>();
        EXPECT_TRUE(sky.Initialize(Device.get()));
        sky.SetSettings(Rendering::SkySettings{}, Rendering::SkySystemState{});

        Nodes::RegisterEngineNodeTypes(Registry);
        return true;
    }

    RenderPipelineBlueprint Blueprint() const
    {
        const std::filesystem::path path = std::filesystem::path(GE_RENDERER_REPO_ROOT) /
                                           "Engine/Assets/RenderPipelines/ForwardPlus.rendergraph";
        std::ifstream in(path);
        EXPECT_TRUE(in.is_open()) << path;
        std::stringstream text;
        text << in.rdbuf();
        const nlohmann::json pipeline = nlohmann::json::parse(text.str());

        RenderPipelineBlueprint bp;
        bp.pipelineName = "SkyBackdrop";
        bp.worldColorResolveTargetRef = "VolumetricClouds";
        bp.resources.push_back({"VolumetricClouds", kTextureResource});
        bp.resources.push_back({"HDRHeightFog", kTextureResource});
        for (const char* id : {"ViewParamsUpload", "HeightFogParamsUpload", "Sky", "HeightFog"})
            bp.passes.push_back(Step(ShippedPass(pipeline, id)));
        bp.outputs.push_back({"FinalColor", "View.Resolve"});
        return bp;
    }

    void Down()
    {
        Device->WaitForIdle();
        Rs->Shutdown();
        Device->Shutdown();
    }
};

struct FogReads
{
    bool FogDeclared = false;   // the fog pass is in the graph at all
    bool ReadsBackdrop = false; // the fog pass samples the texture the sky published
    std::string Accesses;       // every resource the fog pass touches, for the failure message
};

FogReads DeclareFrame(SkyBackdropFixture& f)
{
    RenderPipelineInstance instance(*f.Rs, f.Registry);
    instance.SetBlueprint(f.Blueprint());

    RenderGraph::RGResourcePool persistent(f.Device.get());
    RenderGraph::RGTransientPool transient(f.Device.get());
    RenderGraph::RGUploadRing ring(f.Device.get(), 2, 262144);
    RenderGraph::RGFrame frame(f.Device.get(), &persistent, &transient, &ring);
    frame.BeginFrame(0);
    const RenderGraph::RGTexture color = frame.ImportPersistentTexture(
        "SB.Color", ViewTargetDesc(TextureFormat::RGBA8_UNORM,
                                   static_cast<uint32_t>(TextureUsage::RenderTarget | TextureUsage::ShaderResource)));
    const RenderGraph::RGTexture depth = frame.ImportPersistentTexture(
        "SB.Depth", ViewTargetDesc(TextureFormat::D32_FLOAT,
                                   static_cast<uint32_t>(TextureUsage::DepthStencil | TextureUsage::ShaderResource)));
    f.Rs->BeginWorldDrawFrame();
    f.Rs->BuildWorldBatchKeys();
    const std::vector<ViewTargetsRG> targets = {ViewTargetsRG{f.View, color, depth, {}}};
    const std::vector<Rendering::ViewDesc> views(f.Rs->Views().GetViews().begin(), f.Rs->Views().GetViews().end());
    instance.Declare(frame, targets, views);

    // The backdrop texture is identified by what the sky's own draw samples: the per-view import
    // dedups by handle onto the frame-scope LUT import, so its name is not the published one.
    FogReads out;
    const RenderGraph::RGGraph& g = frame.Graph();
    const auto isSampled = [](const RenderGraph::RGAccessRecord& a)
    { return a.Access == RenderGraph::RGAccess::Sampled; };
    std::vector<std::string> skySamples;
    for (const RenderGraph::RGAccessRecord& a : g.Accesses())
    {
        const char* pn = g.PassName(a.Pass);
        const char* rn = g.ResourceName(a.Resource);
        if (pn && rn && isSampled(a) && std::string(pn).find(".Sky.") != std::string::npos)
            skySamples.emplace_back(rn);
    }
    for (const RenderGraph::RGAccessRecord& a : g.Accesses())
    {
        const char* pn = g.PassName(a.Pass);
        const char* rn = g.ResourceName(a.Resource);
        if (!pn || !rn || std::string(pn).find("HeightFog") == std::string::npos)
            continue;
        out.FogDeclared = true;
        out.Accesses += std::string(pn) + ":" + rn + " ";
        if (isSampled(a) && std::find(skySamples.begin(), skySamples.end(), rn) != skySamples.end())
            out.ReadsBackdrop = true;
    }
    if (skySamples.empty())
        ADD_FAILURE() << "the sky's draw sampled nothing: no backdrop to compare against";
    frame.MarkOutput(color);
    frame.Execute();
    f.Device->WaitForIdle();
    return out;
}

} // namespace

// Sky colour mode: the fog pass samples the backdrop SkyRender published. Without the publication
// the input does not resolve and the fog pass declines, so the haze would vanish in this mode.
TEST(RenderPipelineDeclareTests, HeightFogInTheSkyColorModeReadsTheSkysBackdrop)
{
    SkyBackdropFixture f;
    if (!f.Up(Components::HeightFogColorMode::SkyHorizon))
        GTEST_SKIP() << "No Vulkan device available";
    const FogReads r = DeclareFrame(f);
    EXPECT_TRUE(r.FogDeclared) << "the fog pass must run in the sky colour mode";
    EXPECT_TRUE(r.ReadsBackdrop) << "the fog pass must sample SkyRender's published backdrop; it touches "
                                 << r.Accesses;
    f.Down();
}

// Fixed colour mode: the gate binds a placeholder, so the fog pass has no edge to the sky's texture.
TEST(RenderPipelineDeclareTests, HeightFogInTheFixedColorModeHasNoEdgeToTheSkysBackdrop)
{
    SkyBackdropFixture f;
    if (!f.Up(Components::HeightFogColorMode::FixedColor))
        GTEST_SKIP() << "No Vulkan device available";
    const FogReads r = DeclareFrame(f);
    EXPECT_TRUE(r.FogDeclared) << "the fog pass must run in the fixed colour mode";
    EXPECT_FALSE(r.ReadsBackdrop) << "the fixed colour mode must not wait on the sky's texture; it touches "
                                  << r.Accesses;
    f.Down();
}
