#include<vulkan/vulkan.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderbufferInfo.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKTextureCreateInfo.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKSurface.h>
#include<hgl/Macro.h>
#include<hgl/type/String.h>

namespace hgl::graph{

namespace
{
    VkSwapchainKHR CreateVulkanSwapChain(const VulkanDevAttr *dev_attr, const VkExtent2D &extent, VkSwapchainKHR old_swapchain = VK_NULL_HANDLE)
    {
        uint32_t image_count;

        const auto &surface_caps=dev_attr->surface->GetCapabilities();

        image_count=surface_caps.minImageCount;
        if(image_count<3)
            image_count=3;

        // maxImageCount == 0 means "no maximum" per Vulkan.
        if(surface_caps.maxImageCount>0 && image_count>surface_caps.maxImageCount)
            image_count=surface_caps.maxImageCount;

        VkExtent2D actual_extent;
        if(surface_caps.currentExtent.width != 0xFFFFFFFF && surface_caps.currentExtent.height != 0xFFFFFFFF
           && surface_caps.currentExtent.width > 0 && surface_caps.currentExtent.height > 0)
        {
            actual_extent = surface_caps.currentExtent;
        }
        else
        {
            actual_extent = extent;
        }

        if(actual_extent.width < surface_caps.minImageExtent.width)
            actual_extent.width = surface_caps.minImageExtent.width;
        if(actual_extent.width > surface_caps.maxImageExtent.width)
            actual_extent.width = surface_caps.maxImageExtent.width;
        if(actual_extent.height < surface_caps.minImageExtent.height)
            actual_extent.height = surface_caps.minImageExtent.height;
        if(actual_extent.height > surface_caps.maxImageExtent.height)
            actual_extent.height = surface_caps.maxImageExtent.height;

        VkSwapchainCreateInfoKHR swapchain_ci{};
        swapchain_ci.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext=nullptr;
        swapchain_ci.flags=0;
        swapchain_ci.surface=dev_attr->surface->GetSurface();
        swapchain_ci.minImageCount=image_count;
        swapchain_ci.imageFormat=dev_attr->surface_format.format;
        swapchain_ci.imageColorSpace=dev_attr->surface_format.colorSpace;
        swapchain_ci.imageExtent=actual_extent;
        swapchain_ci.imageArrayLayers=1;
        swapchain_ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.queueFamilyIndexCount=0;
        swapchain_ci.pQueueFamilyIndices=nullptr;
        swapchain_ci.preTransform=dev_attr->surface->GetPreTransform();
        swapchain_ci.compositeAlpha=dev_attr->surface->GetCompositeAlpha();
        swapchain_ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.clipped=VK_TRUE;
        swapchain_ci.oldSwapchain=old_swapchain;

        if(surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if(surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        swapchain_ci.imageSharingMode=VkSharingMode(SharingMode::Exclusive);

        VkSwapchainKHR swap_chain;
        VkResult result=vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain);

        if(result!=VK_SUCCESS && old_swapchain!=VK_NULL_HANDLE)
        {
            GLogWarning("vkCreateSwapchainKHR with oldSwapchain failed (result=%d), retrying with VK_NULL_HANDLE", (int)result);
            swapchain_ci.oldSwapchain=VK_NULL_HANDLE;
            result=vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain);
        }

        if(result!=VK_SUCCESS)
        {
            GLogError("vkCreateSwapchainKHR failed, result=",result);
            return(VK_NULL_HANDLE);
        }

    #ifdef _DEBUG
        if(dev_attr->debug_utils)
            dev_attr->debug_utils->SetSwapchainKHR(swap_chain,"SwapChain");
    #endif//_DEBUG

        return(swap_chain);
    }
}//namespace

bool SwapchainModule::CreateSwapchainFBO(Swapchain *swapchain)
{
    HGL_CAPTURE_SCOPE();

    VkResult sc_result=vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),nullptr);
    if(sc_result!=VK_SUCCESS)
    {
        LogError("vkGetSwapchainImagesKHR(count) failed, result=",sc_result);
        return(false);
    }

    AutoDeleteArray<VkImage> sc_images(swapchain->image_count);

    sc_result=vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),sc_images);
    if(sc_result!=VK_SUCCESS)
    {
        LogError("vkGetSwapchainImagesKHR(list) failed, result=",sc_result);
        return(false);
    }

    swapchain->sc_image=new SwapchainImage[swapchain->image_count]();

    AnsiString num_string;
    VulkanDevice *device=GetDevice();
    auto *dev_attr=GetDevAttr();

    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        num_string=AnsiString("SwapchainColor[")+AnsiString::numberOf(i)+"]";
        U8String color_name=ToU8String(num_string);
        swapchain->sc_image[i].color=tex_manager->CreateTexture2D(new SwapchainColorTextureCreateInfo(swapchain->surface_format.format,swapchain->extent,sc_images[i],color_name));

        if(!swapchain->sc_image[i].color)
            return(false);

        num_string=AnsiString("SwapchainDepth[")+AnsiString::numberOf(i)+"]";
        U8String depth_name=ToU8String(num_string);
        swapchain->sc_image[i].depth=tex_manager->CreateTexture2D(new SwapchainDepthTextureCreateInfo(swapchain->depth_format,swapchain->extent,depth_name));

        if(!swapchain->sc_image[i].depth)
            return(false);

        swapchain->sc_image[i].fbo=rt_manager->CreateFBO(sc_render_pass,
                                                         swapchain->sc_image[i].color->GetImageView(),
                                                         swapchain->sc_image[i].depth->GetImageView());

        AnsiString idx=AnsiString::numberOf(i);
        swapchain->sc_image[i].cmd_buf=device->CreateRenderCommandBuffer(AnsiString("Swapchain_RenderCmdBuffer_")+idx);

    #ifdef _DEBUG
        if(dev_attr->debug_utils)
        {
            dev_attr->debug_utils->SetTexture(swapchain->sc_image[i].color,"SwapchainColor_"+idx);
            dev_attr->debug_utils->SetTexture(swapchain->sc_image[i].depth,"SwapchainDepth_"+idx);
            dev_attr->debug_utils->SetFramebuffer(swapchain->sc_image[i].fbo->GetFramebuffer(),"SwapchainFBO_"+idx);
        }
    #endif//_DEBUG
    }

    return(true);
}

Swapchain *SwapchainModule::CreateSwapchain(const VkExtent2D &extent, VkSwapchainKHR old_swapchain)
{
    HGL_CAPTURE_SCOPE();
    auto *dev_attr=GetDevAttr();

    if(!dev_attr)
    {
        LogError("dev_attr is null");
        return(nullptr);
    }

    Swapchain *swapchain=new Swapchain;

    const auto &surface_caps=dev_attr->surface->GetCapabilities();

    VkExtent2D actual_extent;
    if(surface_caps.currentExtent.width != 0xFFFFFFFF && surface_caps.currentExtent.height != 0xFFFFFFFF
       && surface_caps.currentExtent.width > 0 && surface_caps.currentExtent.height > 0)
    {
        actual_extent = surface_caps.currentExtent;
    }
    else
    {
        actual_extent = extent;
    }

    if(actual_extent.width < surface_caps.minImageExtent.width)
        actual_extent.width = surface_caps.minImageExtent.width;
    if(actual_extent.width > surface_caps.maxImageExtent.width)
        actual_extent.width = surface_caps.maxImageExtent.width;
    if(actual_extent.height < surface_caps.minImageExtent.height)
        actual_extent.height = surface_caps.minImageExtent.height;
    if(actual_extent.height > surface_caps.maxImageExtent.height)
        actual_extent.height = surface_caps.maxImageExtent.height;

    swapchain->device=dev_attr->device;
    swapchain->extent=actual_extent;
    swapchain->transform=surface_caps.currentTransform;
    swapchain->surface_format=dev_attr->surface_format;
    swapchain->depth_format=dev_attr->physical_device->GetDepthFormat();

    swapchain->swap_chain=CreateVulkanSwapChain(dev_attr, actual_extent, old_swapchain);

    if(swapchain->swap_chain)
    {
        if(CreateSwapchainFBO(swapchain))
            return swapchain;

        LogError("CreateSwapchainFBO failed");
    }
    else
    {
        LogError("CreateVulkanSwapChain returned null");
    }

    delete swapchain;
    return(nullptr);
}

bool SwapchainModule::CreateSwapchainRenderTarget(const VkExtent2D &extent, SwapchainRenderTarget *inherit_from)
{
    if(!ecs_context)
        return(false);

    VkSwapchainKHR old_sc = (inherit_from && inherit_from->GetSwapchain()) ? inherit_from->GetSwapchain()->swap_chain : VK_NULL_HANDLE;

    Swapchain *swapchain=CreateSwapchain(extent, old_sc);
    if(!swapchain)
        return(false);

    VulkanDevice *device=GetDevice();
    const uint32_t count=swapchain->image_count;   // slot_count == image_count

    // 主帧的 per-frame 数据槽固定为 [0, HGL_FRAME_SLOT_MAIN)，离屏 RT 的槽带接在其上
    // （见 RenderOptions.h）。image_count 超出即 fail-fast —— 静默重叠会让 prepass
    // 写坏在途主帧的每帧数据（改造前正是靠 RenderTo 全槽排空兜住的隐患）。
    if(count > HGL_FRAME_SLOT_MAIN)
    {
        LogError("[SwapchainModule] image_count=%u 超出主帧数据槽上限 %u —— "
                 "请提高 HGL_FRAME_SLOT_MAIN / HGL_FRAME_SLOT_TOTAL",
                 count, uint32_t(HGL_FRAME_SLOT_MAIN));
        return(false);
    }

    SwapchainFrameSync *sync_slots=new SwapchainFrameSync[count];

    for(uint32_t i=0;i<count;i++)
    {
        AnsiString idx=AnsiString::numberOf(i);
        sync_slots[i].image_available=device->CreateGPUSemaphore("Swapchain:ImageAvailable["+idx+"]");
        sync_slots[i].render_finished=device->CreateGPUSemaphore("Swapchain:RenderFinished["+idx+"]");
        sync_slots[i].queue=device->CreateQueue("Swapchain:Queue["+idx+"]",
                                                1,      // 1 fence per slot
                                                false);
    }

    sc_render_target=new SwapchainRenderTarget(ecs_context,swapchain,sync_slots,count);

    // resize 重建时从旧 RT 继承用户声明（clear_color / env_profile 唯一权威在 RT 上，
    // 不继承的话窗口一缩放清屏色就会回到默认黑）
    if(sc_render_target && inherit_from)
    {
        sc_render_target->SetClearColor(inherit_from->GetClearColor());
        sc_render_target->SetEnvironmentProfile(inherit_from->GetEnvironmentProfile());
    }

    if(ecs_context)
        ecs_context->SetRenderTarget(sc_render_target);

    return true;
}

SwapchainModule::SwapchainModule(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm)
    :GraphModuleInherit<SwapchainModule,GraphModule>(gc,"SwapchainModule")
{
    HGL_CAPTURE_SCOPE();
    tex_manager=tm;
    rt_manager=rtm;
    rp_manager=rpm;
    ecs_context=ecs_ctx;

    auto *dev_attr=GetDevAttr();

    SwapchainRenderbufferInfo rbi(dev_attr->surface_format.format,dev_attr->physical_device->GetDepthFormat());
    sc_render_pass=rp_manager->AcquireRenderPass(&rbi);

#ifdef _DEBUG
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->SetRenderPass(sc_render_pass->GetVkRenderPass(),"SwapchainRenderPass");
#endif//_DEBUG

    VkExtent2D init_extent = dev_attr->surface->GetCapabilities().currentExtent;
    if(init_extent.width == 0xFFFFFFFF || init_extent.height == 0xFFFFFFFF || init_extent.width == 0 || init_extent.height == 0)
    {
        init_extent.width = 1280;
        init_extent.height = 720;
    }

    if(!CreateSwapchainRenderTarget(init_extent))
        LogError("SwapchainModule: CreateSwapchainRenderTarget failed");
}

SwapchainModule::~SwapchainModule()
{
    SAFE_CLEAR(sc_render_target);
}

void SwapchainModule::Release()
{
    if(sc_render_target)
    {
        SAFE_CLEAR(sc_render_target);
    }
    // sc_render_pass is managed by RenderPassManager, not deleted here
}

void SwapchainModule::OnResize(const VkExtent2D &extent)
{
    LogInfo("SwapchainModule::OnResize: requested extent=%ux%u", extent.width, extent.height);

    if(extent.width == 0 || extent.height == 0)
        return;

    if(ecs_context)
        ecs_context->SetRenderTarget(nullptr);

    if(auto *device=GetDevice())
        device->WaitIdle();

    // 暂存旧 RT 供新 RT 继承声明值，新 RT 接好后再销毁
    SwapchainRenderTarget *old_rt=sc_render_target;
    sc_render_target=nullptr;

    VulkanSurface *surface=GetSurface();
    if(!surface)
    {
        sc_render_target=old_rt;
        if(ecs_context)
            ecs_context->SetRenderTarget(sc_render_target);
        LogError("SwapchainModule::OnResize: surface is null");
        return;
    }

    surface->RefreshCaps();

    const VkExtent2D &surface_extent=surface->GetCapabilities().currentExtent;
    VkExtent2D target_extent = extent;
    if(surface_extent.width != 0xFFFFFFFF && surface_extent.height != 0xFFFFFFFF
       && surface_extent.width > 0 && surface_extent.height > 0)
    {
        target_extent = surface_extent;
    }

    if(target_extent.width == 0 || target_extent.height == 0)
    {
        sc_render_target=old_rt;
        if(ecs_context)
            ecs_context->SetRenderTarget(sc_render_target);
        LogWarning("SwapchainModule::OnResize: target extent is 0, skipping recreation");
        return;
    }

    if(!CreateSwapchainRenderTarget(target_extent, old_rt))
    {
        // Keep the previous render target usable if the transient resize
        // state does not allow a new Vulkan swapchain to be created yet.
        sc_render_target=old_rt;
        if(ecs_context)
            ecs_context->SetRenderTarget(sc_render_target);
        LogError("SwapchainModule::OnResize: CreateSwapchainRenderTarget failed");
        return;
    }

    LogInfo("SwapchainModule::OnResize: successfully recreated swapchain with extent=%ux%u", target_extent.width, target_extent.height);
    SAFE_CLEAR(old_rt);
}

bool SwapchainModule::GetSwapchainSize(VkExtent2D *ext)const
{
    if(!ext||!sc_render_target)
        return(false);

    *ext=sc_render_target->GetExtent();
    return(true);
}

bool SwapchainModule::AcquireNextImage()const
{
    if(!sc_render_target)
        return(false);

    return sc_render_target->NextFrame();
}

}//namespace hgl::graph
