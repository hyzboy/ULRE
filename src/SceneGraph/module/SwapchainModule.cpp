#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKNamespace.h>
#include <hgl/vk/VKRenderbufferInfo.h>
#include <hgl/vk/VKRenderTargetData.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VKTextureCreateInfo.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/vk/VKFrameData.h>
#include <hgl/vk/VKSwapchainData.h>
#include <cstdint>
#include <vulkan/vulkan_core.h>
#include <hgl/Macro.h>
#include <hgl/type/Smart.h>
#include <hgl/type/String.h>
#include <hgl/graph/GraphTypes.h>
#include <hgl/graph/module/GraphModule.h>
#include <hgl/vk/VK.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKSurface.h>


VK_NAMESPACE_BEGIN
namespace
{
    //VkExtent2D SwapchainExtentClamp(const VkSurfaceCapabilitiesKHR &surface_caps,const VkExtent2D &acquire_extent)
    //{
    //    VkExtent2D swapchain_extent;

    //    swapchain_extent.width  =hgl_clamp(acquire_extent.width,    surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width   );
    //    swapchain_extent.height =hgl_clamp(acquire_extent.height,   surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height  );

    //    return swapchain_extent;
    //}

    VkSwapchainKHR CreateVulkanSwapChain(const VulkanDevAttr *dev_attr)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        uint32_t image_count;

        const auto &surface_caps=dev_attr->surface->GetCapabilities();

        if(surface_caps.maxImageCount<3)
            image_count=surface_caps.maxImageCount;
        else
            if(surface_caps.maxImageCount>3)
                image_count=3;
            else
                image_count=surface_caps.minImageCount;

        swapchain_ci.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext=nullptr;
        swapchain_ci.flags=0;
        swapchain_ci.surface=dev_attr->surface->GetSurface();
        swapchain_ci.minImageCount=image_count;
        swapchain_ci.imageFormat=dev_attr->surface_format.format;
        swapchain_ci.imageColorSpace=dev_attr->surface_format.colorSpace;
        swapchain_ci.imageExtent=surface_caps.currentExtent;
        swapchain_ci.imageArrayLayers=1;
        swapchain_ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.queueFamilyIndexCount=0;
        swapchain_ci.pQueueFamilyIndices=nullptr;
        swapchain_ci.preTransform=dev_attr->surface->GetPreTransform();
        swapchain_ci.compositeAlpha=dev_attr->surface->GetCompositeAlpha();
        swapchain_ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.clipped=VK_TRUE;
        swapchain_ci.oldSwapchain=VK_NULL_HANDLE;

        if(surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if(surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        swapchain_ci.imageSharingMode=VkSharingMode(SharingMode::Exclusive);

        VkSwapchainKHR swap_chain;
        VkResult result;

        result=vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain);

        if(result!=VK_SUCCESS)
        {
            //LogError(OS_TEXT("vkCreateSwapchainKHR failed, result = ")+OSString(result));
//            os_err<<"vkCreateSwapchainKHR failed, result="<<result<<std::endl;

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
    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->image_count);

    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),sc_images)!=VK_SUCCESS)
        return(false);

    swapchain->sc_image=new SwapchainImage[swapchain->image_count]();  // 使用 new[] 而不是 hgl_zero_new（因为有析构函数）

    AnsiString num_string;
    VulkanDevice *device=GetDevice();
    auto *dev_attr=GetDevAttr();

    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        num_string=AnsiString("SwapchainColor[")+AnsiString::numberOf(i)+"]";
        U8String color_name = ToU8String(num_string);
        swapchain->sc_image[i].color=tex_manager->CreateTexture2D(new SwapchainColorTextureCreateInfo(swapchain->surface_format.format,swapchain->extent,sc_images[i],color_name));

        if(!swapchain->sc_image[i].color)
            return(false);

        num_string=AnsiString("SwapchainDepth[")+AnsiString::numberOf(i)+"]";
        U8String depth_name = ToU8String(num_string);
        swapchain->sc_image[i].depth=tex_manager->CreateTexture2D(new SwapchainDepthTextureCreateInfo(swapchain->depth_format,swapchain->extent,depth_name));

        if(!swapchain->sc_image[i].depth)
            return(false);

        swapchain->sc_image[i].fbo=rt_manager->CreateFBO(sc_render_pass,
                                                         swapchain->sc_image[i].color->GetImageView(),
                                                         swapchain->sc_image[i].depth->GetImageView());

        AnsiString num_string=AnsiString::numberOf(i);

        swapchain->sc_image[i].cmd_buf=device->CreateRenderCommandBuffer(AnsiString("Swapchain_RenderCmdBuffer_")+num_string);

    #ifdef _DEBUG
        if(dev_attr->debug_utils)
        {
            dev_attr->debug_utils->SetTexture(swapchain->sc_image[i].color,"SwapchainColor_"+num_string);
            dev_attr->debug_utils->SetTexture(swapchain->sc_image[i].depth,"SwapchainDepth_"+num_string);

            dev_attr->debug_utils->SetFramebuffer(swapchain->sc_image[i].fbo->GetFramebuffer(),"SwapchainFBO_"+num_string);
        }
    #endif//_DEBUG
    }

    return(true);
}

Swapchain *SwapchainModule::CreateSwapchain()
{
    auto *dev_attr=GetDevAttr();

    if(!dev_attr)
        return(nullptr);

    Swapchain *swapchain=new Swapchain;

    const auto &surface_caps=dev_attr->surface->GetCapabilities();

    swapchain->device=dev_attr->device;
    swapchain->extent=surface_caps.currentExtent;
    swapchain->transform=surface_caps.currentTransform;
    swapchain->surface_format=dev_attr->surface_format;
    swapchain->depth_format=dev_attr->physical_device->GetDepthFormat();

    swapchain->swap_chain=CreateVulkanSwapChain(dev_attr);

    if(swapchain->swap_chain)
    {
        if(CreateSwapchainFBO(swapchain))
            return swapchain;
    }

    delete swapchain;
    swapchain=nullptr;
    return(nullptr);
}

namespace
{
    struct SwapchainRenderTargetData:public RenderTargetData
    {
        void Clear() override
        {
            RenderTargetData::Clear();
        }
    };//
}//namespace

bool SwapchainModule::CreateSwapchainRenderTarget()
{
    if(!ecs_context)
        return(false);

    Swapchain *swapchain=CreateSwapchain();

    if(!swapchain)
        return(false);

    VulkanDevice *device=GetDevice();

    SwapchainRenderTargetData *rtd_list=new SwapchainRenderTargetData[swapchain->image_count];
    SwapchainRenderTargetData *rtd=rtd_list;

    SwapchainImage *sc_image=swapchain->sc_image;

    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        rtd->fbo=sc_image->fbo;
        rtd->queue=device->CreateQueue("SwapchainImage", swapchain->image_count, false);
        rtd->render_complete_semaphore=device->CreateGPUSemaphore("SwapchainImage:RenderComplete");

        rtd->cmd_buf=sc_image->cmd_buf;

        rtd->color_count=1;
        rtd->color_textures=new Texture2D*[1];
        rtd->color_textures[0]=sc_image->color;
        rtd->depth_texture=sc_image->depth;

        ++rtd;
        ++sc_image;
    }

    sc_render_target=new SwapchainRenderTarget(ecs_context,
                                               swapchain,
                                               device->CreateGPUSemaphore("Swapchain:ImageAcquired"),
                                               rtd_list
    );

    return true;
}

SwapchainModule::~SwapchainModule()
{
    // 删除SwapchainRenderTarget对象
    SAFE_CLEAR(sc_render_target);
}

void SwapchainModule::Release()
{
    // SwapchainModule is responsible for cleaning up resources it created:
    // 1. Swapchain object
    // 2. present_complete_semaphore
    //
    // These must be released BEFORE the SwapchainRenderTarget is deleted
    
    if (sc_render_target)
    {
        // 1. Release frame resources (clears references, doesn't delete owned objects)
        sc_render_target->ReleaseFrameResources();
        
        // 2. Release swapchain-specific resources (Swapchain and Semaphore)
        sc_render_target->ReleaseSwapchainResources();
        
        // 3. Now safe to delete the render target object
        // Its destructor will only delete the rtd_list array and clear references
        SAFE_CLEAR(sc_render_target);
    }
}

SwapchainModule::SwapchainModule(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm)
    :GraphModuleInherit<SwapchainModule,GraphModule>(gc,"SwapchainModule")
{
    tex_manager=tm;
    rt_manager=rtm;
    rp_manager=rpm;
    ecs_context=ecs_ctx;

    auto *dev_attr=GetDevAttr();

    SwapchainRenderbufferInfo rbi(dev_attr->surface_format.format,dev_attr->physical_device->GetDepthFormat());

    sc_render_pass=rp_manager->AcquireRenderPass(&rbi);

#ifdef _DEBUG
    {
        if(dev_attr->debug_utils)
            dev_attr->debug_utils->SetRenderPass(sc_render_pass->GetVkRenderPass(),"SwapchainRenderPass");
    }
#endif//_DEBUG

    if(!CreateSwapchainRenderTarget())
        return;
}

void SwapchainModule::OnResize(const VkExtent2D &extent)
{
    SAFE_CLEAR(sc_render_target)

        VulkanSurface *surface=GetSurface();

    surface->RefreshCaps();

    CreateSwapchainRenderTarget();
}
//
//RenderCmdBuffer *SwapchainModule::BeginRender()
//{
//    return sc_render_target->AcquireNextImage();
//}
//
//void SwapchainModule::EndRender()
//{
//    sc_render_target->Submit();
//    sc_render_target->PresentBackbuffer();
//    sc_render_target->WaitQueue();
//    sc_render_target->WaitFence();
//}

bool                    SwapchainModule::GetSwapchainSize(VkExtent2D *ext)const
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

// ============================================
// NEW ARCHITECTURE METHODS
// ============================================

bool SwapchainModule::Initialize()
{
    if (!swapchain_data)
    {
        swapchain_data = new SwapchainData();
    }

    // Create the raw Vulkan swapchain
    Swapchain *vk_swapchain = CreateSwapchain();
    if (!vk_swapchain)
    {
        return false;
    }

    swapchain_data->swapchain = vk_swapchain->swap_chain;
    swapchain_data->image_format = vk_swapchain->surface_format.format;
    swapchain_data->extent = vk_swapchain->extent;
    swapchain_data->frame_count = vk_swapchain->image_count;

    // Allocate frame array
    swapchain_data->frames.resize(vk_swapchain->image_count);

    // Extract handles from SwapchainImage array
    VulkanDevice *device = GetDevice();
    SwapchainImage *sc_image = vk_swapchain->sc_image;

    for (uint32_t i = 0; i < vk_swapchain->image_count; i++)
    {
        FrameResources &frame = swapchain_data->frames[i];

        frame.frame_index = i;

        // Extract image and view from color texture
        if (sc_image->color)
        {
            frame.vk_image = sc_image->color->GetImage();
            frame.image_view = sc_image->color->GetVulkanImageView();
        }

        // Extract framebuffer
        if (sc_image->fbo)
        {
            frame.framebuffer = sc_image->fbo->GetFramebuffer();
        }

        // Get render pass if available
        frame.render_pass = sc_render_pass ? sc_render_pass->GetVkRenderPass() : VK_NULL_HANDLE;

        // Extract command buffer (RenderCmdBuffer can be cast to VkCommandBuffer)
        if (sc_image->cmd_buf)
        {
            frame.cmd_buffer = (VkCommandBuffer)(*sc_image->cmd_buf);
        }

        // Create or get queue (DeviceQueue wrapper managed by SwapchainModule)
        if (device)
        {
            frame.queue = device->CreateQueue("SwapchainFrame", vk_swapchain->image_count, false);
        }

        // Create synchronization primitives (Semaphore and Fence managed by SwapchainModule)
        if (device)
        {
            frame.image_acquired_semaphore = device->CreateGPUSemaphore("Swapchain:ImageAcquired");
            frame.render_complete_semaphore = device->CreateGPUSemaphore("Swapchain:RenderComplete");
            frame.fence = device->CreateFence("Swapchain:Fence");
        }

        frame.image_index = i;

        ++sc_image;
    }

    return true;
}

bool SwapchainModule::CreatePerFrameResources(SwapchainData &sc_data)
{
    // Frame resources are now populated directly in Initialize()
    // where we have access to the Swapchain object and can properly create
    // DeviceQueue, Semaphore, and Fence wrapper objects for each frame.
    //
    // This method is kept for API compatibility but the actual resource
    // creation is done during Initialize().
    
    if (sc_data.frames.empty())
    {
        return false;
    }

    return true;
}

bool SwapchainModule::DestroyPerFrameResources(SwapchainData &sc_data)
{
    VulkanDevice *device = GetDevice();
    if (!device)
    {
        return false;
    }

    for (auto &frame : sc_data.frames)
    {
        // FrameResources::Clear() handles deletion of wrapper objects owned by SwapchainModule:
        // - DeviceQueue*
        // - Semaphore* (both image_acquired and render_complete)
        // - Fence*
        //
        // Other resources (VkImage, VkImageView, VkFramebuffer, VkRenderPass, VkCommandBuffer)
        // are owned by their respective managers and will be deleted by them.

        frame.Clear();
    }

    sc_data.frames.clear();
    return true;
}

FrameResources *SwapchainModule::GetCurrentFrame() const
{
    if (!swapchain_data || swapchain_data->frames.empty())
    {
        return nullptr;
    }

    return &const_cast<SwapchainData *>(swapchain_data)->GetCurrentFrame();
}

FrameResources *SwapchainModule::GetFrame(uint32_t index) const
{
    if (!swapchain_data || index >= swapchain_data->frame_count)
    {
        return nullptr;
    }

    return &const_cast<SwapchainData *>(swapchain_data)->GetFrame(index);
}

VK_NAMESPACE_END
