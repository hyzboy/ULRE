#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKSemaphore.h>

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

    VkSwapchainKHR CreateVulkanSwapChain(const VkDevAttr *dev_attr)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        uint32_t image_count;

        if(dev_attr->surface_caps.maxImageCount<3)
            image_count=dev_attr->surface_caps.maxImageCount;
        else
        if(dev_attr->surface_caps.maxImageCount>3)
            image_count=3;
        else
            image_count=dev_attr->surface_caps.minImageCount;

        swapchain_ci.sType                  =VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext                  =nullptr;
        swapchain_ci.flags                  =0;
        swapchain_ci.surface                =dev_attr->surface;
        swapchain_ci.minImageCount          =image_count;
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

bool SwapchainModule::CreateSwapchainFBO(Swapchain *swapchain)
{
    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->image_count);

    if(vkGetSwapchainImagesKHR(swapchain->device,swapchain->swap_chain,&(swapchain->image_count),sc_images)!=VK_SUCCESS)
        return(false);
    
    swapchain->sc_image=hgl_zero_new<SwapchainImage>(swapchain->image_count);

    AnsiString num_string;
    GPUDevice *device=GetDevice();
    auto *dev_attr=GetDeviceAttribute();

    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        swapchain->sc_image[i].color=tex_manager->CreateTexture2D(new SwapchainColorTextureCreateInfo(swapchain->surface_format.format,swapchain->extent,sc_images[i]));

        if(!swapchain->sc_image[i].color)
            return(false);

        swapchain->sc_image[i].depth=tex_manager->CreateTexture2D(new SwapchainDepthTextureCreateInfo(swapchain->depth_format,swapchain->extent));

        if(!swapchain->sc_image[i].depth)
            return(false);

        swapchain->sc_image[i].fbo=rt_manager->CreateFBO(   sc_render_pass,
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
    auto *dev_attr=GetDeviceAttribute();

    if(!dev_attr)
        return(nullptr);

    Swapchain *swapchain=new Swapchain;

    swapchain->device           =dev_attr->device;
    swapchain->extent           =dev_attr->surface_caps.currentExtent;
    swapchain->transform        =dev_attr->surface_caps.currentTransform;
    swapchain->surface_format   =dev_attr->surface_format;
    swapchain->depth_format     =dev_attr->physical_device->GetDepthFormat();

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
            delete render_complete_semaphore;
            delete queue;

            delete[] color_textures;
        }
    };//
}//namespace

bool SwapchainModule::CreateSwapchainRenderTarget()
{
    Swapchain *swapchain=CreateSwapchain();

    if(!swapchain)
        return(false);

    GPUDevice *device=GetDevice();

    SwapchainRenderTargetData *rtd_list=new SwapchainRenderTargetData[swapchain->image_count];
    SwapchainRenderTargetData *rtd=rtd_list;

    SwapchainImage *sc_image=swapchain->sc_image;

    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        rtd->fbo                        =sc_image->fbo;
        rtd->queue                      =device->CreateQueue(swapchain->image_count,false);
        rtd->render_complete_semaphore  =device->CreateGPUSemaphore();

        rtd->cmd_buf                    =sc_image->cmd_buf;

        rtd->color_count                =1;
        rtd->color_textures             =new Texture2D*[1];
        rtd->color_textures[0]          =sc_image->color;
        rtd->depth_texture              =sc_image->depth;

        ++rtd;
        ++sc_image;
    }

    sc_render_target=new SwapchainRenderTarget( GetRenderFramework(),
                                                swapchain,
                                                device->CreateGPUSemaphore(),
                                                rtd_list
                                                );

    return true;
}

SwapchainModule::~SwapchainModule()
{
    SAFE_CLEAR(sc_render_target);
}

SwapchainModule::SwapchainModule(RenderFramework *rf,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm):GraphModuleInherit<SwapchainModule,GraphModule>(rf,"SwapchainModule")
{
    tex_manager=tm;
    rt_manager=rtm;
    rp_manager=rpm;
    
    auto *dev_attr=GetDeviceAttribute();

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

    VkDevAttr *dev_attr=GetDeviceAttribute();

    dev_attr->RefreshSurfaceCaps();

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

VK_NAMESPACE_END
