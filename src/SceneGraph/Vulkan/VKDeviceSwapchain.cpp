#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDeviceRenderPassManage.h>
#include<iostream>

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

    VkSwapchainKHR CreateSwapChain(const GPUDeviceAttribute *dev_attr,const VkExtent2D &extent)
    {
        VkSwapchainCreateInfoKHR swapchain_ci;

        swapchain_ci.sType                  =VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext                  =nullptr;
        swapchain_ci.flags                  =0;
        swapchain_ci.surface                =dev_attr->surface;
        swapchain_ci.minImageCount          =3;//rsa->surface_caps.minImageCount;
        swapchain_ci.imageFormat            =dev_attr->surface_format.format;
        swapchain_ci.imageColorSpace        =dev_attr->surface_format.colorSpace;
        swapchain_ci.imageExtent            =extent;
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
            os_err<<"vkCreateSwapchainKHR failed, result="<<result<<std::endl;

            return(VK_NULL_HANDLE);
        }

    #ifdef _DEBUG
        if(dev_attr->debug_utils)
            dev_attr->debug_utils->SetSwapchainKHR(swap_chain,"SwapChain");
    #endif//_DEBUG

        return(swap_chain);
    }
}//namespace

bool GPUDevice::CreateSwapchainFBO(Swapchain *swapchain)
{
    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->image_count),nullptr)!=VK_SUCCESS)
        return(false);

    AutoDeleteArray<VkImage> sc_images(swapchain->image_count);

    if(vkGetSwapchainImagesKHR(attr->device,swapchain->swap_chain,&(swapchain->image_count),sc_images)!=VK_SUCCESS)
        return(false);

    swapchain->sc_image=hgl_zero_new<SwapchainImage>(swapchain->image_count);

    AnsiString num_string;


    for(uint32_t i=0;i<swapchain->image_count;i++)
    {
        swapchain->sc_image[i].color=CreateTexture2D(new SwapchainColorTextureCreateInfo(swapchain->surface_format.format,swapchain->extent,sc_images[i]));

        if(!swapchain->sc_image[i].color)
            return(false);

        swapchain->sc_image[i].depth =CreateTexture2D(new SwapchainDepthTextureCreateInfo(swapchain->depth_format,swapchain->extent));

        if(!swapchain->sc_image[i].depth)
            return(false);

        swapchain->sc_image[i].fbo=CreateFBO(   swapchain->render_pass,
                                                swapchain->sc_image[i].color->GetImageView(),
                                                swapchain->sc_image[i].depth->GetImageView());

        AnsiString num_string=AnsiString::numberOf(i);

        swapchain->sc_image[i].cmd_buf=CreateRenderCommandBuffer(AnsiString("Swapchain_RenderCmdBuffer_")+num_string);

    #ifdef _DEBUG
        if(attr->debug_utils)
        {
            AnsiString num=AnsiString::numberOf(i);

            attr->debug_utils->SetTexture(swapchain->sc_image[i].color,"SwapchainColor_"+num_string);
            attr->debug_utils->SetTexture(swapchain->sc_image[i].depth,"SwapchainDepth_"+num_string);

            attr->debug_utils->SetFramebuffer(swapchain->sc_image[i].fbo->GetFramebuffer(),"SwapchainFBO_"+num_string);
        }
    #endif//_DEBUG
    }

    return(true);
}

Swapchain *GPUDevice::CreateSwapchain(const VkExtent2D &acquire_extent)
{
    Swapchain *swapchain=new Swapchain;

    swapchain->device           =attr->device;
    swapchain->extent           =acquire_extent;
    swapchain->transform        =attr->surface_caps.currentTransform;
    swapchain->surface_format   =attr->surface_format;
    swapchain->depth_format     =attr->physical_device->GetDepthFormat();
    
    SwapchainRenderbufferInfo rbi(swapchain->surface_format.format,swapchain->depth_format);

    swapchain->render_pass      =render_pass_manage->AcquireRenderPass(&rbi);

    #ifdef _DEBUG
        if(attr->debug_utils)
            attr->debug_utils->SetRenderPass(swapchain->render_pass->GetVkRenderPass(),"MainDeviceRenderPass");
    #endif//_DEBUG

    swapchain->swap_chain       =CreateSwapChain(attr,acquire_extent);

    if(swapchain->swap_chain)
    if(CreateSwapchainFBO(swapchain))
        return(swapchain);

    delete swapchain;
    swapchain=nullptr;

    return(nullptr);
}
VK_NAMESPACE_END
