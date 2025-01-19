#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKCommandBuffer.h>

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

    VkSwapchainKHR CreateVulkanSwapChain(const GPUDeviceAttribute *dev_attr)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        swapchain_ci.sType                  =VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext                  =nullptr;
        swapchain_ci.flags                  =0;
        swapchain_ci.surface                =dev_attr->surface;
        swapchain_ci.minImageCount          =dev_attr->surface_caps.minImageCount;
        swapchain_ci.imageFormat            =dev_attr->surface_format.format;
        swapchain_ci.imageColorSpace        =dev_attr->surface_format.colorSpace;
        swapchain_ci.imageExtent            =dev_attr->surface_caps.currentExtent;
        swapchain_ci.imageArrayLayers       =1;
        swapchain_ci.imageUsage             =VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_ci.queueFamilyIndexCount  =0;
        swapchain_ci.pQueueFamilyIndices    =nullptr;
        swapchain_ci.preTransform           =dev_attr->preTransform;
        swapchain_ci.compositeAlpha         =dev_attr->compositeAlpha;
        swapchain_ci.presentMode            =VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.clipped                =VK_TRUE;
        swapchain_ci.oldSwapchain           =VK_NULL_HANDLE;

        if(dev_attr->surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if(dev_attr->surface_caps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchain_ci.imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t queueFamilyIndices[2]={dev_attr->graphics_family, dev_attr->present_family};
        if(dev_attr->graphics_family!=dev_attr->present_family)
        {
            // If the graphics and present queues are from different queue families,
            // we either have to explicitly transfer ownership of images between
            // the queues, or we have to create the swapchain with imageSharingMode
            // as VK_SHARING_MODE_CONCURRENT
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT;
            swapchain_ci.queueFamilyIndexCount=2;
            swapchain_ci.pQueueFamilyIndices=queueFamilyIndices;
        }
        else
        {
            swapchain_ci.imageSharingMode = VkSharingMode(SharingMode::Exclusive);
        }

        VkSwapchainKHR swap_chain;
        VkResult result;

        result=vkCreateSwapchainKHR(dev_attr->device,&swapchain_ci,nullptr,&swap_chain);

        if(result!=VK_SUCCESS)
        {
            //LOG_ERROR(OS_TEXT("vkCreateSwapchainKHR failed, result = ")+OSString(result));
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

bool SwapchainModule::CreateSwapchainFBO()
{
    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->color_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->color_count);

    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->color_count),sc_images)!=VK_SUCCESS)
        return(false);

    {
        auto sc_depth_tci=new SwapchainDepthTextureCreateInfo(GetPhysicalDevice()->GetDepthFormat(),swapchain->extent);

        swapchain->sc_depth =tex_manager->CreateTexture2D(sc_depth_tci);

        if(!swapchain->sc_depth)
            return(false);
    }

    //#ifdef _DEBUG
    //    if(dev_attr->debug_utils)
    //    {
    //        dev_attr->debug_utils->SetImage(swapchain->sc_depth->GetImage(),"SwapchainDepthImage");
    //        dev_attr->debug_utils->SetImageView(swapchain->sc_depth->GetVulkanImageView(),"SwapchainDepthImageView");
    //        dev_attr->debug_utils->SetDeviceMemory(swapchain->sc_depth->GetDeviceMemory(),"SwapchainDepthMemory");
    //    }
    //#endif//_DEBUG

    swapchain->sc_color =hgl_zero_new<Texture2D *>(swapchain->color_count);
    swapchain->sc_fbo   =hgl_zero_new<Framebuffer *>(swapchain->color_count);

    for(uint32_t i=0;i<swapchain->color_count;i++)
    {
        auto sc_color_tci=new SwapchainColorTextureCreateInfo(swapchain->surface_format.format,swapchain->extent,sc_images[i]);

        swapchain->sc_color[i]=tex_manager->CreateTexture2D(sc_color_tci);

        if(!swapchain->sc_color[i])
            return(false);

        swapchain->sc_fbo[i]=rt_manager->CreateFBO(swapchain_rp,
                                                    swapchain->sc_color[i]->GetImageView(),
                                                    swapchain->sc_depth->GetImageView());

    //#ifdef _DEBUG
    //    if(dev_attr->debug_utils)
    //    {
    //        dev_attr->debug_utils->SetImage(swapchain->sc_color[i]->GetImage(),"SwapchainColorImage_"+AnsiString::numberOf(i));
    //        dev_attr->debug_utils->SetImageView(swapchain->sc_color[i]->GetVulkanImageView(),"SwapchainColorImageView_"+AnsiString::numberOf(i));

    //        dev_attr->debug_utils->SetFramebuffer(swapchain->sc_fbo[i]->GetFramebuffer(),"SwapchainFBO_"+AnsiString::numberOf(i));
    //    }
    //#endif//_DEBUG
    }

    return(true);
}

bool SwapchainModule::CreateSwapchain()
{
    auto *dev_attr=GetDeviceAttribute();

    if(!dev_attr)
        return(false);

    swapchain=new Swapchain;

    swapchain->device           =dev_attr->device;
    swapchain->extent           =dev_attr->surface_caps.currentExtent;
    swapchain->transform        =dev_attr->surface_caps.currentTransform;
    swapchain->surface_format   =dev_attr->surface_format;
    swapchain->depth_format     =dev_attr->physical_device->GetDepthFormat();

    swapchain->swap_chain=CreateVulkanSwapChain(dev_attr);

    if(swapchain->swap_chain)
    {
        if(CreateSwapchainFBO())
            return swapchain;
    }

    delete swapchain;
    swapchain=nullptr;
    return(false);
}

bool SwapchainModule::CreateSwapchainRenderTarget()
{
    GPUDevice *device=GetDevice();

    DeviceQueue *q=device->CreateQueue(swapchain->color_count,false);
    Semaphore *render_complete_semaphore=device->CreateGPUSemaphore();
    Semaphore *present_complete_semaphore=device->CreateGPUSemaphore();

    swapchain_rt=new RTSwapchain(   device->GetDevice(),
                                    swapchain,
                                    q,
                                    render_complete_semaphore,
                                    present_complete_semaphore,
                                    swapchain_rp
                                    );

    return true;
}

void SwapchainModule::ClearRenderCmdBuffer()
{
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(cmd_buf,swapchain->color_count);
}

void SwapchainModule::InitRenderCmdBuffer()
{
    ClearRenderCmdBuffer();

    cmd_buf=hgl_zero_new<RenderCmdBuffer *>(swapchain->color_count);
    
    GPUDevice *device=GetDevice();

    AnsiString cmd_buf_name;

    for(uint32_t i=0;i<swapchain->color_count;i++)
    {
        cmd_buf_name=device->GetPhysicalDevice()->GetDeviceName()+AnsiString(":RenderCmdBuffer_")+AnsiString::numberOf(i);

        cmd_buf[i]=device->CreateRenderCommandBuffer(cmd_buf_name);
    }
}

SwapchainModule::~SwapchainModule()
{
    ClearRenderCmdBuffer();

    SAFE_CLEAR(swapchain_rp);
    SAFE_CLEAR(swapchain_rt);
    SAFE_CLEAR(swapchain);
}

SwapchainModule::SwapchainModule(GPUDevice *dev,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm):GraphModuleInherit<SwapchainModule,RenderModule>(dev,"SwapchainModule")
{
    tex_manager=tm;
    rt_manager=rtm;

    SwapchainRenderbufferInfo rbi(swapchain->surface_format.format,swapchain->depth_format);

    swapchain_rp=rpm->AcquireRenderPass(&rbi);

    if(!CreateSwapchain())
        return;

    //#ifdef _DEBUG
    //    if(dev_attr->debug_utils)
    //        dev_attr->debug_utils->SetRenderPass(swapchain_rp->GetVkRenderPass(),"SwapchainRenderPass");
    //#endif//_DEBUG

    if(!CreateSwapchainRenderTarget())
        return;

    InitRenderCmdBuffer();
}

void SwapchainModule::OnResize(const VkExtent2D &extent)
{
    ClearRenderCmdBuffer();

    SAFE_CLEAR(swapchain_rt)

    GetDeviceAttribute()->RefreshSurfaceCaps();

    CreateSwapchainRenderTarget();

    InitRenderCmdBuffer();
}

bool SwapchainModule::BeginFrame()
{
    uint32_t index=swapchain_rt->AcquireNextImage();

    if(index>=swapchain->color_count)
        return(false);

    return(true);
}

void SwapchainModule::EndFrame()
{
    int index=swapchain_rt->GetCurrentFrameIndices();

    VkCommandBuffer cb=*(cmd_buf[index]);

    swapchain_rt->Submit(cb);
    swapchain_rt->PresentBackbuffer();
    swapchain_rt->WaitQueue();
    swapchain_rt->WaitFence();
}

RenderCmdBuffer *SwapchainModule::GetRenderCmdBuffer()
{
    int index=swapchain_rt->GetCurrentFrameIndices();
    
    if(index>=swapchain->color_count)
        return(nullptr);

    return cmd_buf[index];
}

VK_NAMESPACE_END
