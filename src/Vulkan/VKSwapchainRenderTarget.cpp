#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/log/Log.h>
#include<chrono>
 //#include<iostream>

namespace hgl::graph{
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

    return (result == VK_SUCCESS);
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

    present_info.waitSemaphoreCount =1;
    present_info.pWaitSemaphores    =&wait_semaphores;
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
            return false;
        }
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
