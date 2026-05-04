#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/log/Log.h>
#include<chrono>
#ifdef _WIN32
#include<Windows.h>
#endif
 //#include<iostream>

namespace hgl::graph{
namespace
{
    bool IsRenderDocAttached()
    {
#ifdef _WIN32
        return GetModuleHandleA("renderdoc.dll") != nullptr;
#else
        return false;
#endif
    }
}

SwapchainRenderTarget::SwapchainRenderTarget(hgl::ecs::ECSContext *ctx,Swapchain *sc,Semaphore *pcs,RenderTargetData *rtl):MultiFrameRenderTarget(ctx,sc->image_count,rtl)
{
    swapchain=sc;

    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &(swapchain->swap_chain);

    present_complete_semaphore=pcs;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    // Do NOT delete present_complete_semaphore or swapchain here
    // They are deleted by SwapchainModule in ReleaseSwapchainResources()
}

bool SwapchainRenderTarget::NextFrame()
{
    auto start = std::chrono::high_resolution_clock::now();
    LogInfo("[SWAPCHAIN] NextFrame START current_frame=%u semaphore=%p", current_frame, (void*)present_complete_semaphore);

    VkResult result = vkAcquireNextImageKHR(GetVkDevice(),
                                 swapchain->swap_chain,
                                 UINT64_MAX,
                                 *present_complete_semaphore,
                                 VK_NULL_HANDLE,
                                 &current_frame);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LogInfo("[SWAPCHAIN] NextFrame END result=%d new_frame=%u time=%lldms", static_cast<int>(result), current_frame, duration);

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        return true;

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        LogWarning("[SWAPCHAIN] NextFrame OUT_OF_DATE");
        return false;
    }

    LogWarning("[SWAPCHAIN] NextFrame FAILED result=%d", static_cast<int>(result));
    return false;
}

bool SwapchainRenderTarget::WaitFence()
{
    // Each RTD currently has its own DeviceQueue wrapper but wrappers share the
    // same VkQueue handle; waiting only current_frame can miss the last submit
    // from another wrapper and cause cross-frame resource hazards.
    for (uint32_t i = 0; i < frame_number; ++i)
    {
        auto *queue = rtd_list[i].queue;
        if (!queue)
            continue;

        if (!queue->WaitLastSubmitFence())
        {
            LogWarning("[SWAPCHAIN] WaitFence FAILED frame=%u", i);
            return false;
        }
    }

    return true;
}

bool SwapchainRenderTarget::WaitQueue()
{
    for (uint32_t i = 0; i < frame_number; ++i)
    {
        auto *queue = rtd_list[i].queue;
        if (!queue)
            continue;

        if (!queue->WaitQueue())
        {
            LogWarning("[SWAPCHAIN] WaitQueue FAILED frame=%u", i);
            return false;
        }
    }

    return true;
}

bool SwapchainRenderTarget::Submit()
{
    auto submit_start = std::chrono::high_resolution_clock::now();
    LogInfo("[SWAPCHAIN] Submit START frame=%u", current_frame);

    RenderTargetData *rtd=rtd_list+current_frame;

    if(!rtd->Submit(present_complete_semaphore))
    {
        LogWarning("[SWAPCHAIN] Submit RenderTargetData::Submit FAILED");
        return(false);
    }

    DeviceQueue *queue=GetQueue();

    VkSemaphore wait_semaphores=*rtd->render_complete_semaphore;

    const bool renderdoc_attached = IsRenderDocAttached();
    if (renderdoc_attached)
    {
        // RenderDoc path: keep standard semaphore-based submit->present ordering.
        // Queue-idle waits can stall under capture overlays.
        LogInfo("[SWAPCHAIN] RenderDoc detected: using semaphore-based present path");
    }

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &wait_semaphores;

    present_info.pImageIndices      =&current_frame;

    auto present_start = std::chrono::high_resolution_clock::now();
    VkResult result=queue->Present(&present_info);
    auto present_end = std::chrono::high_resolution_clock::now();
    auto present_duration = std::chrono::duration_cast<std::chrono::milliseconds>(present_end - present_start).count();

    LogInfo("[SWAPCHAIN] Present result=%d time=%lldms", static_cast<int>(result), present_duration);

    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR)))
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            LogWarning("[SWAPCHAIN] Submit OUT_OF_DATE");
        }
        if (result != VK_ERROR_OUT_OF_DATE_KHR)
        {
            LogWarning("[SWAPCHAIN] Submit FAILED result=%d", static_cast<int>(result));

            if (result == VK_ERROR_DEVICE_LOST)
            {
                // On device-lost paths, waiting this queue fence in the next frame can hang.
                // Clear pending wait state so upper layer can fail fast and recover/restart.
                queue->ClearLastSubmitFenceState();
            }
        }

        return false;
    }

    auto submit_end = std::chrono::high_resolution_clock::now();
    auto submit_duration = std::chrono::duration_cast<std::chrono::milliseconds>(submit_end - submit_start).count();
    LogInfo("[SWAPCHAIN] Submit END total_time=%lldms", submit_duration);

    return(true);
}

void SwapchainRenderTarget::ReleaseSwapchainResources()
{
    // SwapchainModule is responsible for cleaning up Swapchain and present_complete_semaphore
    // Delete them here (called by SwapchainModule::Release())
    SAFE_CLEAR(present_complete_semaphore);
    SAFE_CLEAR(swapchain);
}
}//namespace hgl::graph
