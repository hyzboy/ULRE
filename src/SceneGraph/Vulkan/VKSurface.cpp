#include<hgl/graph/VKSurface.h>

VK_NAMESPACE_BEGIN
using VkQueueFamilyPropertiesList=ArrayList<VkQueueFamilyProperties>;

VulkanSurface::VulkanSurface(const VulkanPhyDevice *phy_dev,VkSurfaceKHR sface)
{
    physical_device=phy_dev;
    surface=sface;
    
    if(!physical_device||!surface)
    {
        return;
    }

    const auto pd=physical_device->GetVulkanDevice();

    {
        const auto &queue_family_properties=physical_device->GetQueueFamilyProperties();

        const int family_count=queue_family_properties.GetCount();

        supports_present=new VkBool32[family_count];

        VkBool32 *sp=supports_present;

        for(int i=0;i<family_count;i++)
        {
            vkGetPhysicalDeviceSurfaceSupportKHR(pd,i,surface,sp);
            ++sp;
        }
    }

    {
        uint32_t count;

        if(vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &count, nullptr) == VK_SUCCESS)
        {
            present_mode_list.Resize(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &count, present_mode_list.GetData());
        }
        else
        {
            present_mode_list.Clear();
        }

        if(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, nullptr) == VK_SUCCESS)
        {
            surface_format_list.Resize(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &count, surface_format_list.GetData());
        }
        else
        {
            surface_format_list.Clear();
        }

        {
            const auto &queue_family_properties=physical_device->GetQueueFamilyProperties();

            const VkQueueFamilyProperties *qfp=queue_family_properties.GetData();

            const int family_count=queue_family_properties.GetCount();

            for(int i=0;i<family_count;i++)
            {
                if(!supports_present[i])
                {
                    ++qfp;
                    continue;
                }

            #define COMP_VK_FAMILY(flag,value) if((value##_family==ERROR_FAMILY_INDEX)&&(qfp->queueFlags&VK_QUEUE_##flag##_BIT))value##_family=i;
            #define COMP_VIDEO_FAMILY(flag,value) if((value##_family==ERROR_FAMILY_INDEX)&&(qfp->queueFlags&VK_QUEUE_VIDEO_##flag##_BIT_KHR))value##_family=i;

                COMP_VK_FAMILY(GRAPHICS,graphics)
                COMP_VK_FAMILY(COMPUTE,compute)
                COMP_VK_FAMILY(TRANSFER,transfer)
                COMP_VIDEO_FAMILY(DECODE,video_decode)
                COMP_VIDEO_FAMILY(ENCODE,video_encode)

            #undef COMP_VIDEO_FAMILY
            #undef COMP_VK_FAMILY

                ++qfp;
            }
        }
    }

    RefreshCaps();
}

VulkanSurface::~VulkanSurface()
{
    delete[] supports_present;

    if(surface!=VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(physical_device->GetVulkanInstance(),surface,nullptr);
    }
}

void VulkanSurface::RefreshCaps()
{
    const auto pd=physical_device->GetVulkanDevice();

    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd,surface,&capabilities);

        if(capabilities.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            preTransform=capabilities.currentTransform;

             if(capabilities.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR          )compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        else if(capabilities.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR         )compositeAlpha=VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        else if(capabilities.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR  )compositeAlpha=VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else if(capabilities.supportedCompositeAlpha&VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR )compositeAlpha=VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        else
            compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }

}
VK_NAMESPACE_END
