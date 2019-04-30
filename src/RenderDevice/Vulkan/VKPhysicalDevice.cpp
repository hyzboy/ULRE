#include<hgl/graph/vulkan/VKPhysicalDevice.h>

VK_NAMESPACE_BEGIN
PhysicalDevice::PhysicalDevice(VkInstance inst,VkPhysicalDevice pd)
{
    instance=inst;
    physical_device=pd;

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceProperties(physical_device,&properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

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
}

const bool PhysicalDevice::CheckMemoryType(uint32_t typeBits,VkFlags requirements_mask,uint32_t *typeIndex)const
{
    // Search memtypes to find first index with those properties
    for(uint32_t i=0; i<memory_properties.memoryTypeCount; i++)
    {
        if((typeBits&1)==1)
        {
            // Type is available, does it match user properties?
            if((memory_properties.memoryTypes[i].propertyFlags&requirements_mask)==requirements_mask)
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
VK_NAMESPACE_END