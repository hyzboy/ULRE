#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/log/Log.h>

namespace hgl::ecs {

RenderSystemCore::RenderSystemCore(ECSContext* ctx)
    : world(ctx), gpu_device(nullptr), render_target(nullptr),
    current_frame(0), swapchain_image_index(0), frame_begun(false), render_pass_begun(false) {
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

    swapchain_image_index = render_target->GetCurrentFrameIndex();

    frame_begun = true;
    render_pass_begun = false;
    return true;
}

bool RenderSystemCore::BeginRenderPass()
{
    if (!frame_begun || !render_cmd)
    {
        LogWarning("RenderSystemCore::BeginRenderPass: frame not ready");
        return false;
    }

    render_cmd->SetClearColor(0, clear_color);
    render_cmd->BeginRenderingDynamic(render_target->GetCurrentRTD());
    render_pass_begun = true;
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
        frame_begun = false;
        render_pass_begun = false;
        return;
    }

    if (render_cmd && render_pass_begun)
        render_cmd->EndRenderingDynamic(render_target->GetCurrentRTD());

    render_target->EndRender();

    current_frame++;
    render_cmd = nullptr;
    frame_begun = false;
    render_pass_begun = false;
}

} // namespace hgl::ecs
