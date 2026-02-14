#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/graph/data/CameraInfo.h>
#include <hgl/graph/render/RenderCmdBuffer.h>
#include <hgl/log/Log.h>

namespace hgl::ecs {

RenderSystemCore::RenderSystemCore(ECSContext* ctx)
    : world(ctx), gpu_device(nullptr), render_target(nullptr), 
      current_frame(0), swapchain_image_index(0), frame_begun(false) {
    if (!world) {
        LOG_ERROR("RenderSystemCore: ECSContext is null");
    }
}

RenderSystemCore::~RenderSystemCore() {
    // 等待 GPU 彻底完成
    if (gpu_device) {
        auto vk_device = gpu_device->GetDevice();
        vkDeviceWaitIdle(vk_device);
    }
    
    // 清理 Vulkan 同步原语
    if (gpu_device) {
        auto vk_device = gpu_device->GetDevice();
        
        for (auto fence : frame_fences) {
            vkDestroyFence(vk_device, fence, nullptr);
        }
        
        for (auto sem : image_available_semaphores) {
            vkDestroySemaphore(vk_device, sem, nullptr);
        }
        
        for (auto sem : render_finished_semaphores) {
            vkDestroySemaphore(vk_device, sem, nullptr);
        }
        
        frame_fences.clear();
        image_available_semaphores.clear();
        render_finished_semaphores.clear();
    }
}

bool RenderSystemCore::Initialize() {
    if (!world) {
        LOG_ERROR("RenderSystemCore::Initialize: ECSContext is null");
        return false;
    }
    
    gpu_device = world->GetGPUDevice();
    render_target = world->GetRenderTarget();
    
    if (!gpu_device || !render_target) {
        LOG_ERROR("RenderSystemCore::Initialize: gpu_device or render_target is null");
        return false;
    }
    
    auto vk_device = gpu_device->GetDevice();
    
    // 创建同步原语
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始化为已信号状态
    
    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    // 为每一帧创建同步原语
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFence fence;
        if (vkCreateFence(vk_device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
            LOG_ERROR("RenderSystemCore::Initialize: Failed to create frame fence #{}", i);
            return false;
        }
        frame_fences.push_back(fence);
        
        VkSemaphore image_sem, render_sem;
        if (vkCreateSemaphore(vk_device, &sem_info, nullptr, &image_sem) != VK_SUCCESS) {
            LOG_ERROR("RenderSystemCore::Initialize: Failed to create image available semaphore #{}", i);
            return false;
        }
        
        if (vkCreateSemaphore(vk_device, &sem_info, nullptr, &render_sem) != VK_SUCCESS) {
            LOG_ERROR("RenderSystemCore::Initialize: Failed to create render finished semaphore #{}", i);
            return false;
        }
        
        image_available_semaphores.push_back(image_sem);
        render_finished_semaphores.push_back(render_sem);
    }
    
    // 创建命令缓冲区
    render_cmd = std::make_unique<hgl::graph::RenderCmdBuffer>(gpu_device);
    if (!render_cmd) {
        LOG_ERROR("RenderSystemCore::Initialize: Failed to create RenderCmdBuffer");
        return false;
    }
    
    LOG_INFO("RenderSystemCore initialized successfully (MAX_FRAMES_IN_FLIGHT={})", MAX_FRAMES_IN_FLIGHT);
    return true;
}

bool RenderSystemCore::BeginFrame() {
    if (!render_target) {
        LOG_ERROR("RenderSystemCore::BeginFrame: render_target is null");
        return false;
    }
    
    if (frame_begun) {
        LOG_WARNING("RenderSystemCore::BeginFrame: frame already begun");
        return false;
    }
    
    auto vk_device = gpu_device->GetDevice();
    auto queue = gpu_device->GetGraphicsQueue();
    
    // 计算当前帧的索引
    uint32_t frame_idx = current_frame % MAX_FRAMES_IN_FLIGHT;
    
    // 等待上一帧完成
    VkResult wait_result = vkWaitForFences(vk_device, 1, &frame_fences[frame_idx], VK_TRUE, UINT64_MAX);
    if (wait_result != VK_SUCCESS) {
        LOG_ERROR("RenderSystemCore::BeginFrame: vkWaitForFences failed with code {}", static_cast<int>(wait_result));
        return false;
    }
    
    VkResult reset_result = vkResetFences(vk_device, 1, &frame_fences[frame_idx]);
    if (reset_result != VK_SUCCESS) {
        LOG_ERROR("RenderSystemCore::BeginFrame: vkResetFences failed with code {}", static_cast<int>(reset_result));
        return false;
    }
    
    // 获取下一个 Swapchain 图像
    VkResult acquire_result = vkAcquireNextImageKHR(
        vk_device, 
        render_target->GetVkSwapchain(), 
        UINT64_MAX, 
        image_available_semaphores[frame_idx],
        VK_NULL_HANDLE,
        &swapchain_image_index
    );
    
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_WARNING("RenderSystemCore::BeginFrame: Swapchain out of date, need to recreate");
        return false;
    } else if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR) {
        LOG_ERROR("RenderSystemCore::BeginFrame: Failed to acquire next swapchain image, result={}", 
                  static_cast<int>(acquire_result));
        return false;
    }
    
    // 开始记录渲染命令
    render_cmd->Begin(swapchain_image_index);
    
    // 设置当前渲染命令缓冲区到 ECSContext
    world->current_render_cmd = render_cmd.get();
    
    frame_begun = true;
    return true;
}

void RenderSystemCore::EndFrame() {
    if (!frame_begun) {
        LOG_WARNING("RenderSystemCore::EndFrame: frame not begun");
        return;
    }
    
    if (!render_cmd) {
        LOG_ERROR("RenderSystemCore::EndFrame: render_cmd is null");
        frame_begun = false;
        return;
    }
    
    // 停止记录命令
    render_cmd->End();
    
    auto vk_device = gpu_device->GetDevice();
    auto queue = gpu_device->GetGraphicsQueue();
    auto present_queue = gpu_device->GetPresentQueue();
    uint32_t frame_idx = current_frame % MAX_FRAMES_IN_FLIGHT;
    
    // 提交命令缓冲区
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = render_cmd->GetVkCommandBuffer();
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available_semaphores[frame_idx];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphores[frame_idx];
    
    VkPipelineStageFlags wait_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit_info.pWaitDstStageMask = &wait_flags;
    
    VkResult submit_result = vkQueueSubmit(queue, 1, &submit_info, frame_fences[frame_idx]);
    if (submit_result != VK_SUCCESS) {
        LOG_ERROR("RenderSystemCore::EndFrame: Failed to submit render command buffer, result={}", 
                  static_cast<int>(submit_result));
        frame_begun = false;
        return;
    }
    
    // Present
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores[frame_idx];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &render_target->GetVkSwapchain();
    present_info.pImageIndices = &swapchain_image_index;
    
    VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        LOG_WARNING("RenderSystemCore::EndFrame: Swapchain presentation resulted in out-of-date/suboptimal");
    } else if (present_result != VK_SUCCESS) {
        LOG_ERROR("RenderSystemCore::EndFrame: vkQueuePresentKHR failed with result={}", static_cast<int>(present_result));
    }
    
    // 清除当前命令缓冲区
    world->current_render_cmd = nullptr;
    
    // 推进到下一帧
    current_frame++;
    frame_begun = false;
}

} // namespace hgl::ecs
