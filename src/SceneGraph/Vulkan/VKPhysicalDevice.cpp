#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKInstance.h>

VK_NAMESPACE_BEGIN
namespace
{
    void debug_queue_family_properties_out(const char *front,const List<VkQueueFamilyProperties> &qfp_list)
    {
        constexpr char *queue_bit_name[]=
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

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);
    vkGetPhysicalDeviceProperties(physical_device,&properties);

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

const bool GPUPhysicalDevice::CheckMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties,uint32_t *typeIndex)const
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