#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/graph/CameraInfo.h>
#include <hgl/graph/camera/ViewportInfo.h>
#include <hgl/vk/VKBufferUpdateQueue.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
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

    bool swapchain_ok = true;
    bool swapchain_system_present = false;
    if (world)
    {
        world->RenderSwapchainNextImage(0.0f);
        if (auto swapchain_system = world->GetSystem<SwapchainNextImageSystem>())
        {
            swapchain_system_present = true;
            swapchain_ok = swapchain_system->WasSuccessful();
        }
    }

    if (!swapchain_system_present)
    {
        if (auto swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget*>(render_target))
        {
            swapchain_ok = swapchain_rt->NextFrame();
        }
    }

    if (!swapchain_ok)
    {
        LogWarning("RenderSystemCore::BeginFrame: Swapchain NextFrame failed");
        return false;
    }

    if (world)
        world->RenderPreBeginFrame(0.0f);

    const VkExtent2D &ext = render_target->GetExtent();
    const auto *vp_info = render_target->GetViewportInfo();
    if (vp_info && (vp_info->GetViewport().x != ext.width || vp_info->GetViewport().y != ext.height))
    {
        render_target->OnResize(ext);
    }

    render_cmd = render_target->BeginRender();
    if (!render_cmd)
    {
        LogError("RenderSystemCore::BeginFrame: BeginRender failed");
        return false;
    }

    if (world)
    {
        world->SetFrameIndex(render_target->GetCurrentFrameIndex());
        world->RenderBeginFrame(0.0f);

        auto camera_system = world->GetSystem<CameraSystem>();
        if (camera_system)
            camera_system->SyncCameraUBO();

        auto environment_system = world->GetSystem<EnvironmentSystem>();
        if (environment_system)
            environment_system->SyncSkyUBO();

        world->RenderPostBeginFrame(0.0f);

        if (camera_system)
            camera_system->BindDescriptor(render_cmd);
    }

    if (auto *device = render_target->GetDevice())
    {
        auto *update_queue = device->GetBufferUpdateQueue();
        if (update_queue && update_queue->HasPendingUpdates())
        {
            update_queue->FlushAll(render_cmd->operator VkCommandBuffer());

            VkMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;

            vkCmdPipelineBarrier(render_cmd->operator VkCommandBuffer(),
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 1, &barrier, 0, nullptr, 0, nullptr);
        }
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
        frame_begun = false;
        return;
    }

    if (render_cmd)
        render_cmd->EndRenderPass();

    render_target->EndRender();

    bool submit_ok = false;
    bool submit_system_present = false;
    if (world)
    {
        world->RenderSubmit(0.0f);
        if (auto submit_system = world->GetSystem<SwapchainSubmitSystem>())
        {
            submit_system_present = true;
            submit_ok = submit_system->WasSuccessful();
        }
    }

    if (!submit_system_present)
    {
        submit_ok = render_target->Submit();
    }

    if (!submit_ok)
        LogError("RenderSystemCore::EndFrame: Submit failed");

    // 推进到下一帧
    current_frame++;
    render_cmd = nullptr;
    frame_begun = false;
}

} // namespace hgl::ecs
