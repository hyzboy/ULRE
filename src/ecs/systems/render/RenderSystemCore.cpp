#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/log/Log.h>

namespace hgl::ecs {

RenderSystemCore::RenderSystemCore(ECSContext* ctx)
    : world(ctx), gpu_device(nullptr), render_target(nullptr),
      current_frame(0), swapchain_image_index(0), frame_begun(false) {
    if (!world) {
        LogError("RenderSystemCore: ECSContext is null");
    }
}

RenderSystemCore::~RenderSystemCore() {
    if (gpu_device)
        gpu_device->WaitIdle();
}

bool RenderSystemCore::Initialize() {
    if (!world) {
        LogError("RenderSystemCore::Initialize: ECSContext is null");
        return false;
    }

    gpu_device = world->GetGPUDevice();
    render_target = world->GetRenderTarget();

    if (!gpu_device || !render_target) {
        LogError("RenderSystemCore::Initialize: gpu_device or render_target is null");
        return false;
    }

    LogInfo("RenderSystemCore initialized successfully");
    return true;
}

bool RenderSystemCore::BeginFrame() {
    // 每次BeginFrame时重新获取render_target，因为窗口resize时可能被重建
    if (world) {
        render_target = world->GetRenderTarget();
    }

    if (!render_target) {
        LogError("RenderSystemCore::BeginFrame: render_target is null");
        return false;
    }

    if (frame_begun) {
        LogWarning("RenderSystemCore::BeginFrame: frame already begun");
        return false;
    }

    render_cmd = render_target->BeginRender();
    if (!render_cmd)
    {
        LogError("RenderSystemCore::BeginFrame: BeginRender failed");
        return false;
    }

    if (world)
        world->SetCurrentRenderCmd(render_cmd);

    if (world)
    {
        world->PrepareRenderPassSetup(render_target->GetCurrentFrameIndex(), 0.0f);
    }

    render_cmd->SetClearColor(0, clear_color);
    render_cmd->BeginRenderPass();

    swapchain_image_index = render_target->GetCurrentFrameIndex();

    frame_begun = true;
    return true;
}

void RenderSystemCore::EndFrame() {
    if (!frame_begun) {
        LogWarning("RenderSystemCore::EndFrame: frame not begun");
        return;
    }

    // 重新获取render_target以防resize时被重建
    if (world) {
        render_target = world->GetRenderTarget();
    }

    if (!render_target) {
        LogError("RenderSystemCore::EndFrame: render_target is null");
        if (world)
            world->SetCurrentRenderCmd(nullptr);
        frame_begun = false;
        return;
    }

    if (render_cmd)
        render_cmd->EndRenderPass();

    render_target->EndRender();

    // 推进到下一帧
    if (world)
        world->SetCurrentRenderCmd(nullptr);

    current_frame++;
    render_cmd = nullptr;
    frame_begun = false;
}

} // namespace hgl::ecs
