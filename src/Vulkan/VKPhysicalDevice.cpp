#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKInstance.h>
#include<hgl/vk/VKPipelineConfig.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h>
#include"DebugOutProperties.h"

namespace hgl::graph{
namespace
{
    void debug_queue_family_properties_out(const char *front,const ValueArray<VkQueueFamilyProperties> &qfp_list)
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
            AnsiString flags;
            uint32_t bits=p->queueFlags;

            for(uint j=0;j<7;j++)
            {
                if(bits&1)
                {
                    if(!flags.IsEmpty()) flags += ",";
                    flags += queue_bit_name[j];
                    bits>>=1;
                }
                else
                {
                    bits>>=1;
                }
            }

            GLogInfo("%s Queue Family [%d] count: %u, timestampValidBits: %u, minImageTransferGranularity [%u,%u,%u], queueFlags[%s]",
                     front, i, p->queueCount, p->timestampValidBits,
                     p->minImageTransferGranularity.width, p->minImageTransferGranularity.height, p->minImageTransferGranularity.depth,
                     flags.c_str());

            ++p;
        }
    }
}

VulkanPhyDevice::VulkanPhyDevice(VkInstance inst,VkPhysicalDevice pd)
{
    instance=inst;
    physical_device=pd;

    // First, get basic properties to detect the API version
    vkGetPhysicalDeviceProperties(physical_device,&properties);

    // Extract Vulkan version from apiVersion: version = VK_VERSION_MAJOR(apiVersion).VK_VERSION_MINOR(apiVersion)
    const uint32_t api_version = properties.apiVersion;
    const uint32_t version_major = VK_API_VERSION_MAJOR(api_version);
    const uint32_t version_minor = VK_API_VERSION_MINOR(api_version);

    {
        mem_zero(features11);
        mem_zero(features12);
        mem_zero(features13);
        mem_zero(features14);

        auto func=(PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceFeatures2KHR");

        if(func)
        {
            VkPhysicalDeviceFeatures2 features2;

            features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
            features2.pNext=nullptr;

            // Build feature chain based on device's supported API version
            void** ppNext=&features2.pNext;

            // VK_KHR_16bit_storage（Vulkan 1.0 提升）：独立结构，未并入 11/12/13/14
            features16.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
            features16.pNext=nullptr;
            *ppNext=&features16;
            ppNext=&features16.pNext;

            if(version_major > 1 || version_minor >= 1)
            {
                features11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
                features11.pNext=nullptr;
                *ppNext=&features11;
                ppNext=&features11.pNext;
            }

            if(version_major > 1 || version_minor >= 2)
            {
                features12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
                features12.pNext=nullptr;
                *ppNext=&features12;
                ppNext=&features12.pNext;
            }

            if(version_major > 1 || version_minor >= 3)
            {
                features13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                features13.pNext=nullptr;
                *ppNext=&features13;
                ppNext=&features13.pNext;
            }

            if(version_major > 1 || version_minor >= 4)
            {
                features14.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
                features14.pNext=nullptr;
                *ppNext=&features14;
                ppNext=&features14.pNext;
            }

            // VK_EXT_mesh_shader（独立扩展结构——mesh/task shader 特性）
            mesh_shader_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
            mesh_shader_features.pNext=nullptr;
            *ppNext=&mesh_shader_features;
            ppNext=&mesh_shader_features.pNext;

            func(physical_device,&features2);

            mem_copy(features,features2.features);
        }
        else
        {
            vkGetPhysicalDeviceFeatures(physical_device,&features);
        }
    }

    {
        mem_zero(properties11);
        mem_zero(properties12);
        mem_zero(properties13);
        mem_zero(properties14);

        auto func=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceProperties2KHR");

        if(func)
        {
            VkPhysicalDeviceProperties2 properties2;

            properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
            properties2.pNext=nullptr;

            // Build properties chain based on device's supported API version
            void** ppNext=&properties2.pNext;

            if(version_major > 1 || version_minor >= 1)
            {
                properties11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
                properties11.pNext=nullptr;
                *ppNext=&properties11;
                ppNext=&properties11.pNext;
            }

            if(version_major > 1 || version_minor >= 2)
            {
                properties12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
                properties12.pNext=nullptr;
                *ppNext=&properties12;
                ppNext=&properties12.pNext;
            }

            if(version_major > 1 || version_minor >= 3)
            {
                properties13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES;
                properties13.pNext=nullptr;
                *ppNext=&properties13;
                ppNext=&properties13.pNext;
            }

            if(version_major > 1 || version_minor >= 4)
            {
                properties14.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
                properties14.pNext=nullptr;
                *ppNext=&properties14;
            }

            func(physical_device,&properties2);

            mem_copy(properties,properties2.properties);
        }
    }

    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    // Detect Resizable BAR support
    // ReBAR is available if there's a large HOST_VISIBLE + DEVICE_LOCAL memory heap
    rebar_size = 0;
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        const VkMemoryType& type = memory_properties.memoryTypes[i];
        const VkMemoryPropertyFlags required_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if ((type.propertyFlags & required_flags) == required_flags)
        {
            // Found a memory type with both HOST_VISIBLE and DEVICE_LOCAL
            // Check if the heap size is large enough (ReBAR typically exposes full VRAM)
            const VkMemoryHeap& heap = memory_properties.memoryHeaps[type.heapIndex];

            // If heap is > 512MB, it's likely ReBAR (traditional BAR is 256MB)
            if (heap.size > (512ULL * 1024 * 1024))
            {
                rebar_size = heap.size;
                break;
            }
        }
    }

    std::string debug_front="PhysicalDevice["+std::string(properties.deviceName)+" v"+std::to_string(version_major)+"."+std::to_string(version_minor)+"]";

    {
        uint32_t property_count;

        vkEnumerateDeviceLayerProperties(physical_device,&property_count,nullptr);

        layer_properties.Resize(property_count);
        vkEnumerateDeviceLayerProperties(physical_device,&property_count,layer_properties.GetData());

        // Log Vulkan API version support
        GLogInfo("%s supported Vulkan API version: %d.%d",
                 debug_front.c_str(), version_major, version_minor);

        debug_out(debug_front.c_str(),layer_properties);
    }

    {
        uint32_t exten_count;

        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,nullptr);

        extension_properties.Resize(exten_count);
        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,extension_properties.GetData());

        debug_out(debug_front.c_str(),extension_properties);
    }

    {
        uint32_t family_count;

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);

        queue_family_properties.Resize(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,queue_family_properties.GetData());

        debug_queue_family_properties_out(debug_front.c_str(),queue_family_properties);
    }

    if(features14.indexTypeUint8)
        support_u8_index=true;
    else
        support_u8_index=CheckExtensionSupport(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);

    // VK_EXT_mesh_shader：mesh shader 为必用路径，特性值仅作诊断记录
    GLogInfo("%s mesh shader: (taskShader=%d meshShader=%d meshShaderQueries=%d)",
             debug_front.c_str(),
             mesh_shader_features.taskShader,
             mesh_shader_features.meshShader,
             mesh_shader_features.meshShaderQueries);

    dynamic_state=CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    graphics_pipeline_library=false;
    mem_zero(graphics_pipeline_library_features);
    mem_zero(graphics_pipeline_library_properties);

    if(!FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY
    && CheckExtensionSupport(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME)
    && CheckExtensionSupport(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME))
    {
        auto get_features2=(PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceFeatures2KHR");
        if(get_features2)
        {
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
            graphics_pipeline_library_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
            graphics_pipeline_library_features.pNext=nullptr;
            features2.pNext=&graphics_pipeline_library_features;
            get_features2(physical_device,&features2);
        }

        auto get_properties2=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceProperties2KHR");
        if(get_properties2)
        {
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
            graphics_pipeline_library_properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_PROPERTIES_EXT;
            graphics_pipeline_library_properties.pNext=nullptr;
            properties2.pNext=&graphics_pipeline_library_properties;
            get_properties2(physical_device,&properties2);
        }

        graphics_pipeline_library=(graphics_pipeline_library_features.graphicsPipelineLibrary==VK_TRUE);
    }

    physical_device_profile = mtl::contract::BuildPhysicalDeviceProfileFromVulkanPhyDevice(*this);

}

VulkanPhyDevice::~VulkanPhyDevice()
{
}

const bool VulkanPhyDevice::GetLayerVersion(const AnsiString &name,uint32_t &spec,uint32_t &impl)const
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

const uint32_t VulkanPhyDevice::GetExtensionVersion(const AnsiString &name)const
{
    for(const VkExtensionProperties &ep:extension_properties)
    {
        if(name.Comp(ep.extensionName)==0)
            return ep.specVersion;
    }

    return 0;
}

const bool VulkanPhyDevice::CheckExtensionSupport(const AnsiString &name)const
{
    for(const VkExtensionProperties &ep:extension_properties)
    {
        if(name.Comp(ep.extensionName)==0)
            return(true);
    }

    return(false);
}

const int VulkanPhyDevice::GetMemoryType(uint32_t typeBits,VkMemoryPropertyFlags properties)const
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

VkFormat VulkanPhyDevice::GetDepthFormat(bool lower_to_high)const
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

VkFormat VulkanPhyDevice::GetDepthStencilFormat(bool lower_to_high)const
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
}//namespace hgl::graph

