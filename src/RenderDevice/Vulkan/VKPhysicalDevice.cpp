#include<hgl/graph/vulkan/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
PhysicalDevice::PhysicalDevice(VkInstance inst,VkPhysicalDevice pd)
{
    instance=inst;
    physical_device=pd;

    {
        uint32_t property_count;

        vkEnumerateDeviceLayerProperties(physical_device,&property_count,nullptr);

        layer_properties.SetCount(property_count);
        vkEnumerateDeviceLayerProperties(physical_device,&property_count,layer_properties.GetData());

        debug_out(layer_properties);
    }

    {
        uint32_t exten_count;

        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,nullptr);

        extension_properties.SetCount(exten_count);
        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,extension_properties.GetData());

        debug_out(extension_properties);
    }

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2=nullptr;

    if(GetExtensionSpecVersion(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        GetPhysicalDeviceProperties2=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2KHR");

    if(!GetPhysicalDeviceProperties2)
        if(GetExtensionSpecVersion(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
            GetPhysicalDeviceProperties2=(PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2");

    if(GetPhysicalDeviceProperties2)
    {
        VkPhysicalDeviceProperties2KHR properties2;
        GetPhysicalDeviceProperties2(physical_device,&properties2);

        hgl_cpy(properties,properties2.properties);

        if(properties2.pNext)
            memcpy(&driver_properties,properties2.pNext,sizeof(VkPhysicalDeviceDriverPropertiesKHR));
    }
    else
    {
        vkGetPhysicalDeviceProperties(physical_device,&properties);

        hgl_zero(driver_properties);
    }

    InitFormatSupport();
}

const uint32_t PhysicalDevice::GetExtensionSpecVersion(const UTF8String &name)const
{
    const uint count=extension_properties.GetCount();
    const VkExtensionProperties *ep=extension_properties.GetData();

    for(uint i=0;i<count;i++)
    {
        if(name.Comp(ep->extensionName)==0)
            return ep->specVersion;
    }

    return 0;
}

const bool PhysicalDevice::CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex)const
{
    // Search memtypes to find first index with those properties
    for(uint32_t i=0; i<memory_properties.memoryTypeCount; i++)
    {
        if((typeBits&1)==1)
        {
            // Type is available, does it match user properties?
            if((memory_properties.memoryTypes[i].propertyFlags&properties)==properties)
            {
                *typeIndex=i;
                return true;
            }
        }
        typeBits>>=1;
    }
    // No memory types matched, return failure
    return false;
}

void PhysicalDevice::InitFormatSupport()
{
    VkFormatProperties formatProps;

    for(uint32 i=VK_FORMAT_BEGIN_RANGE;i<=VK_FORMAT_END_RANGE;i++)
    {        
        hgl_zero(formatProps);

        vkGetPhysicalDeviceFormatProperties(physical_device,(VkFormat)i,&formatProps);

        if(formatProps.optimalTilingFeatures    & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT        )optimal_color_format.Add((VkFormat)i);
        if(formatProps.linearTilingFeatures     & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT        )linear_color_format.Add((VkFormat)i);
        if(formatProps.bufferFeatures           & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT        )buffer_color_format.Add((VkFormat)i);

        if(formatProps.optimalTilingFeatures    & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)optimal_depth_format.Add((VkFormat)i);
        if(formatProps.linearTilingFeatures     & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)linear_depth_format.Add((VkFormat)i);
        if(formatProps.bufferFeatures           & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)buffer_depth_format.Add((VkFormat)i);
    }    
}

VkFormat PhysicalDevice::GetDepthFormat(bool lower_to_high)const
{
    constexpr VkFormat depthFormats[] =
    {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    VkFormat result=VK_FORMAT_UNDEFINED;

    for (auto& format : depthFormats)
    {
        if(IsOptimalDepthFormat(format))
        {
            if(lower_to_high)
                return format;
            else
                result=format;
        }
    }

    return result;
}

VkFormat PhysicalDevice::GetDepthStencilFormat(bool lower_to_high)const
{
    constexpr VkFormat depthStencilFormats[] =
    {
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT
    };

    VkFormat result=VK_FORMAT_UNDEFINED;

    for (auto& format : depthStencilFormats)
    {
        if(IsOptimalDepthFormat(format))
        {
            if(lower_to_high)
                return format;
            else
                result=format;
        }
    }

    return result;
}
VK_NAMESPACE_END