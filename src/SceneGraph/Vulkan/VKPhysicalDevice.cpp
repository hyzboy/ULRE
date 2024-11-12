#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKInstance.h>
#include"DebugOutProperties.h"

VK_NAMESPACE_BEGIN
namespace
{
    void debug_queue_family_properties_out(const char *front,const List<VkQueueFamilyProperties> &qfp_list)
    {
        constexpr const char *queue_bit_name[]=
        {
            "Graphics",
            "Compute",
            "Transfer",
            "SparseBinding",
            "Protected",
            "VideoDecode",
            "VideoEncode"
        };

        const int count=qfp_list.GetCount();

        if(count<=0)return;

        const VkQueueFamilyProperties *p=qfp_list.GetData();

        for(int i=0;i<count;i++)
        {
            std::cout<<front<<" Queue Family ["<<i<<"] count: "<<p->queueCount
                <<", timestampValidBits: "<<p->timestampValidBits
                <<", minImageTransferGranularity [" <<p->minImageTransferGranularity.width<<","
                                                    <<p->minImageTransferGranularity.height<<","
                                                    <<p->minImageTransferGranularity.depth<<"], queueFlags[";

            uint32_t bits=p->queueFlags;

            for(uint i=0;i<7;i++)
            {
                if(bits&1)
                {
                    std::cout<<queue_bit_name[i];
                    bits>>=1;
                    if(bits>0)std::cout<<",";
                }
                else
                {
                    bits>>=1;
                }
            }                

            std::cout<<"]"<<std::endl;

            ++p;
        }
    }
}

GPUPhysicalDevice::GPUPhysicalDevice(VkInstance inst,VkPhysicalDevice pd)
{
    instance=inst;
    physical_device=pd;

    {
        hgl_zero(features11);
        hgl_zero(features12);
        hgl_zero(features13);

        auto func=(PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceFeatures2KHR");

        if(func)
        {
            VkPhysicalDeviceFeatures2 features2;
            VkPhysicalDeviceBlendOperationAdvancedFeaturesEXT boaf;

            hgl_zero(features2);
            hgl_zero(boaf);

            features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
            features2.pNext=&features11;

            features11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features11.pNext=&features12;

            features12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            features12.pNext=&features13;

            features13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            features13.pNext=&boaf;

            boaf.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_FEATURES_EXT;
            boaf.pNext=nullptr;

            func(physical_device,&features2);

            hgl_cpy(features,features2.features);

            blendOpAdvanced=boaf.advancedBlendCoherentOperations;
        }
        else
        {
            vkGetPhysicalDeviceFeatures(physical_device,&features);
        }
    }

    {
        hgl_zero(properties11);
        hgl_zero(properties12);
        hgl_zero(properties13);
        hgl_zero(blendOpAdvProperties);

        auto func=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceProperties2KHR");

        if(func)
        {
            VkPhysicalDeviceProperties2 properties2;

            hgl_zero(properties2);

            properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
            properties2.pNext=&properties11;

            properties11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            properties11.pNext=&properties12;

            properties12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
            properties12.pNext=&properties13;

            properties13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
            properties13.pNext=&blendOpAdvProperties;

            blendOpAdvProperties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BLEND_OPERATION_ADVANCED_PROPERTIES_EXT;
            blendOpAdvProperties.pNext=nullptr;

            func(physical_device,&properties2);

            hgl_cpy(properties,properties2.properties);
        }
        else
        {
            vkGetPhysicalDeviceProperties(physical_device,&properties);
        }
    }

    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    std::string debug_front="PhysicalDevice["+std::string(properties.deviceName)+"]";

    {
        uint32_t property_count;

        vkEnumerateDeviceLayerProperties(physical_device,&property_count,nullptr);

        layer_properties.SetCount(property_count);
        vkEnumerateDeviceLayerProperties(physical_device,&property_count,layer_properties.GetData());

        debug_out(debug_front.c_str(),layer_properties);
    }

    {
        uint32_t exten_count;

        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,nullptr);

        extension_properties.SetCount(exten_count);
        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,extension_properties.GetData());

        debug_out(debug_front.c_str(),extension_properties);
    }

    {
        uint32_t family_count;

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);

        queue_family_properties.SetCount(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,queue_family_properties.GetData());

        debug_queue_family_properties_out(debug_front.c_str(),queue_family_properties);
    }

    support_u8_index=CheckExtensionSupport(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
    dynamic_state=CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
}

const bool GPUPhysicalDevice::GetLayerVersion(const AnsiString &name,uint32_t &spec,uint32_t &impl)const
{    
    for(const VkLayerProperties &lp:layer_properties)
    {
        if(name.Comp(lp.layerName)==0)
        {
            spec=lp.specVersion;
            impl=lp.implementationVersion;

            return(true);
        }
    }

    return(false);
}

const uint32_t GPUPhysicalDevice::GetExtensionVersion(const AnsiString &name)const
{
    for(const VkExtensionProperties &ep:extension_properties)
    {
        if(name.Comp(ep.extensionName)==0)
            return ep.specVersion;
    }

    return 0;
}

const bool GPUPhysicalDevice::CheckExtensionSupport(const AnsiString &name)const
{
    for(const VkExtensionProperties &ep:extension_properties)
    {
        if(name.Comp(ep.extensionName)==0)
            return(true);
    }

    return(false);
}

const int GPUPhysicalDevice::GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties)const
{
    // Search memtypes to find first index with those properties
    for(uint32_t i=0; i<memory_properties.memoryTypeCount; i++)
    {
        if(typeBits&1)  // Type is available, does it match user properties?
            if((memory_properties.memoryTypes[i].propertyFlags&properties)==properties)
                return i;

        typeBits>>=1;
    }

    // No memory types matched, return failure
    return -1;
}

VkFormat GPUPhysicalDevice::GetDepthFormat(bool lower_to_high)const
{
    constexpr VkFormat depthFormats[] =
    {
        PF_D16UN,
        PF_X8_D24UN,
        PF_D16UN_S8U,
        PF_D24UN_S8U,
        PF_D32F,
        PF_D32F_S8U
    };

    VkFormat result=VK_FORMAT_UNDEFINED;

    for (auto& format : depthFormats)
    {
        if(IsDepthAttachmentOptimal(format))
        {
            if(lower_to_high)
                return format;
            else
                result=format;
        }
    }

    return result;
}

VkFormat GPUPhysicalDevice::GetDepthStencilFormat(bool lower_to_high)const
{
    constexpr VkFormat depthStencilFormats[] =
    {
        PF_D16UN_S8U,
        PF_D24UN_S8U,
        PF_D32F_S8U
    };

    VkFormat result=VK_FORMAT_UNDEFINED;

    for (auto& format : depthStencilFormats)
    {
        if(IsDepthAttachmentOptimal(format))
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