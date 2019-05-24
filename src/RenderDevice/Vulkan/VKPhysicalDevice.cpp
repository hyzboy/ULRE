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