#include "Engine/Rendering/Camera.h"

#include "Components/Transform.h"
#include "Engine/Rendering/ViewRegistry.h"
#include "ECS/ECSTemplates.h"
#include "Mathematics/Angle.h"
#include "Mathematics/MatrixOps.h"
#include "Rendering/Common/Math.h"

#include <algorithm>

namespace GameEngine::Engine::Renderer { using namespace ::GameEngine::Rendering; }

namespace GameEngine::Engine::Renderer {
using namespace ::GameEngine::Rendering;

Mathematics::Matrix4x4 Camera::ComputeViewMatrix() const
{
    return Mathematics::Inverse(worldTransform);
}

Mathematics::Matrix4x4 Camera::ComputeProjectionMatrix(float aspectRatio,
                                                       std::optional<float> orthoHeightOverride) const
{
    const float safeAspect = std::max(aspectRatio, 0.0001f);
    if (params.Perspective)
    {
        const float fovRad = Mathematics::ToRadians(params.FovY);
        const float squeeze = std::clamp(params.AnamorphicSqueeze, 1.0f, 4.0f);
        return Mathematics::MakePerspectiveLH_ZO_ReverseZ(fovRad, safeAspect * squeeze, params.NearZ, params.FarZ);
    }

    const float requestedHeight = orthoHeightOverride.value_or(params.OrthographicSize);
    const float orthoHeight = std::max(requestedHeight, 0.0001f);
    const float halfH = orthoHeight * 0.5f;
    const float halfW = halfH * safeAspect;
    return Mathematics::MakeOrthographicLH_ZO_ReverseZ(-halfW, halfW, -halfH, halfH, params.NearZ, params.FarZ);
}

Mathematics::Matrix4x4 Camera::ComputeViewProjectionMatrix(float aspectRatio,
                                                           std::optional<float> orthoHeightOverride) const
{
    return ComputeProjectionMatrix(aspectRatio, orthoHeightOverride) * ComputeViewMatrix();
}

Rendering::CameraData Camera::ToCameraData(float aspectRatio,
                                           std::optional<float> orthoHeightOverride) const
{
    const Mathematics::Matrix4x4 viewM = ComputeViewMatrix();
    const Mathematics::Matrix4x4 projM = ComputeProjectionMatrix(aspectRatio, orthoHeightOverride);
    const Mathematics::Matrix4x4 viewProjM = projM * viewM;

    Rendering::CameraData data{};
    data.view = viewM;
    data.proj = projM;
    data.viewProj = viewProjM;

    // Extract camera world position from the worldTransform translation column.
    const float* wt = worldTransform.Data();
    data.cameraPos.x = wt[12];
    data.cameraPos.y = wt[13];
    data.cameraPos.z = wt[14];
    data.cameraPos.w = 0.0f;

    return data;
}

std::optional<Camera> FindActiveCamera(ECS::World& world)
{
    Camera result{};
    bool found = false;

    auto q = world.Query<ECS::Read<Components::Camera>>();
    q.Each([&](ECS::EntityHandle e, const Components::Camera& cam)
    {
        if (found)
            return;

        // Extract world transform (prefer WorldTransform, fallback to Transform).
        // A WorldTransform the hierarchy has not resolved yet (Version 0 beside
        // a Transform) holds no pose; the camera's own Transform does.
        const auto* worldXf = world.GetComponent<Components::WorldTransform>(e);
        const bool hasLocal = world.HasComponent<Components::Transform>(e);
        if (worldXf && (worldXf->Version != 0u || !hasLocal))
        {
            result.worldTransform = Mathematics::Matrix4x4::FromColumnMajor(worldXf->matrix);
        }
        else if (hasLocal)
        {
            if (auto* xf = world.GetComponent<Components::Transform>(e))
                result.worldTransform = Mathematics::Matrix4x4::FromColumnMajor(xf->matrix);
        }
        result.params = cam;
        result.entity = e;
        found = true;
    });

    if (!found)
        return std::nullopt;
    return result;
}

void DeclareGameCameraView(ViewRegistry& views, Rendering::ViewId view, const Camera* activeCamera)
{
    if (!activeCamera)
        return;
    views.RequestViewFrame(view);
    views.SetViewRenderLayerMask(view, activeCamera->params.VisibleLayers);
}

} // namespace GameEngine::Engine::Renderer
