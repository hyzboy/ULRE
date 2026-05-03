#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKInstance.h>
#include<hgl/log/Log.h>
#include<hgl/shadergen/device/DeviceProfileAdapter.h>
#include"DebugOutProperties.h"

namespace hgl::graph{
namespace
{
    void debug_queue_family_properties_out(const char *front,const std::vector<VkQueueFamilyProperties> &qfp_list)
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

        const int count=(int)qfp_list.size();

        if(count<=0)return;

        const VkQueueFamilyProperties *p=qfp_list.data();

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

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    bool ProbeMeshShaderFeaturesRobust(VkInstance instance,
                                       VkPhysicalDevice physical_device,
                                       VkPhysicalDeviceMeshShaderFeaturesEXT &mesh_features)
    {
        mesh_features = {};
        mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

        if (!instance || !physical_device)
            return false;

#ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
        {
            uint32_t extension_count = 0;
            if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr) != VK_SUCCESS || extension_count == 0)
                return false;

            std::vector<VkExtensionProperties> ext_props(extension_count);
            if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, ext_props.data()) != VK_SUCCESS)
                return false;

            bool has_mesh_extension = false;
            for (const VkExtensionProperties &ep : ext_props)
            {
                if (std::strcmp(ep.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0)
                {
                    has_mesh_extension = true;
                    break;
                }
            }

            if (!has_mesh_extension)
                return false;
        }
#endif

        VkBool32 merged_mesh = VK_FALSE;
        VkBool32 merged_task = VK_FALSE;
        bool queried = false;

        if (auto core_query = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2")))
        {
            VkPhysicalDeviceMeshShaderFeaturesEXT probe{};
            probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &probe;

            core_query(physical_device, &features2);

            if (probe.meshShader == VK_TRUE)
                merged_mesh = VK_TRUE;
            if (probe.taskShader == VK_TRUE)
                merged_task = VK_TRUE;

            mesh_features = probe;
            queried = true;
        }

        if (auto khr_query = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR")))
        {
            VkPhysicalDeviceMeshShaderFeaturesEXT probe{};
            probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &probe;

            khr_query(physical_device, &features2);

            if (probe.meshShader == VK_TRUE)
                merged_mesh = VK_TRUE;
            if (probe.taskShader == VK_TRUE)
                merged_task = VK_TRUE;

            mesh_features = probe;
            queried = true;
        }

        mesh_features.meshShader = merged_mesh;
        mesh_features.taskShader = merged_task;
        return queried;
    }
#endif
}

VulkanPhyDevice::VulkanPhyDevice(VkInstance inst,VkPhysicalDevice pd)
{
    instance=inst;
    physical_device=pd;

    bool graphics_pipeline_library_feature=false;

    // First, get basic properties to detect the API version
    vkGetPhysicalDeviceProperties(physical_device,&properties);

    // Extract Vulkan version from apiVersion: version = VK_VERSION_MAJOR(apiVersion).VK_VERSION_MINOR(apiVersion)
    const uint32_t api_version = properties.apiVersion;
    const uint32_t version_major = VK_API_VERSION_MAJOR(api_version);
    const uint32_t version_minor = VK_API_VERSION_MINOR(api_version);

    {
        mem_zero(features);
        mem_zero(features11);
        mem_zero(features12);
        mem_zero(features13);
        mem_zero(features14);

    #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
        mem_zero(mesh_shader_features);
    #endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
        VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library_features{};
#endif

        if(version_major > 1 || version_minor >= 1)
        {
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext=nullptr;

            void **ppNext=&features2.pNext;

            features11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            features11.pNext=nullptr;
            *ppNext=&features11;
            ppNext=&features11.pNext;

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

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
            mesh_shader_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
            mesh_shader_features.pNext=nullptr;
            *ppNext=&mesh_shader_features;
            ppNext=&mesh_shader_features.pNext;
#endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
            graphics_pipeline_library_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
            graphics_pipeline_library_features.pNext=nullptr;
            *ppNext=&graphics_pipeline_library_features;
            ppNext=&graphics_pipeline_library_features.pNext;
#endif

            // Keep Vulkan 1.4 core feature struct at the tail so older 1.3 layers
            // don't prevent extension feature structs (mesh/GPL) from being observed.
            if(version_major > 1 || version_minor >= 4)
            {
                features14.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
                features14.pNext=nullptr;
                *ppNext=&features14;
                ppNext=&features14.pNext;
            }

            vkGetPhysicalDeviceFeatures2(physical_device,&features2);
            mem_copy(features,features2.features);
        }
        else
        {
            auto func=(PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceFeatures2KHR");

            if(func)
            {
                VkPhysicalDeviceFeatures2 features2{};
                features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                features2.pNext=nullptr;

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
                mesh_shader_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
                mesh_shader_features.pNext=features2.pNext;
                features2.pNext=&mesh_shader_features;
#endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
                graphics_pipeline_library_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
                graphics_pipeline_library_features.pNext=features2.pNext;
                features2.pNext=&graphics_pipeline_library_features;
#endif

                func(physical_device,&features2);
                mem_copy(features,features2.features);
            }
            else
            {
                vkGetPhysicalDeviceFeatures(physical_device,&features);
            }
        }

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
        // Dedicated mesh feature probe keeps mesh/task results stable even when
        // mixed core/extension feature chains behave inconsistently across layers.
        ProbeMeshShaderFeaturesRobust(inst, physical_device, mesh_shader_features);
#endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
        graphics_pipeline_library_feature = (graphics_pipeline_library_features.graphicsPipelineLibrary == VK_TRUE);
#endif
    }

    {
        mem_zero(properties11);
        mem_zero(properties12);
        mem_zero(properties13);
        mem_zero(properties14);

    #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
        mem_zero(mesh_shader_properties);
    #endif

        if(version_major > 1 || version_minor >= 1)
        {
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext=nullptr;

            void **ppNext=&properties2.pNext;

            properties11.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            properties11.pNext=nullptr;
            *ppNext=&properties11;
            ppNext=&properties11.pNext;

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

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
            mesh_shader_properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
            mesh_shader_properties.pNext=nullptr;
            *ppNext=&mesh_shader_properties;
            ppNext=&mesh_shader_properties.pNext;
#endif

            if(version_major > 1 || version_minor >= 4)
            {
                properties14.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES;
                properties14.pNext=nullptr;
                *ppNext=&properties14;
                ppNext=&properties14.pNext;
            }

            vkGetPhysicalDeviceProperties2(physical_device,&properties2);
            mem_copy(properties,properties2.properties);
        }
        else
        {
            auto func=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceProperties2KHR");

            if(func)
            {
                VkPhysicalDeviceProperties2 properties2{};
                properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties2.pNext=nullptr;

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
                mesh_shader_properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
                mesh_shader_properties.pNext=properties2.pNext;
                properties2.pNext=&mesh_shader_properties;
#endif

                func(physical_device,&properties2);
                mem_copy(properties,properties2.properties);
            }
            else
            {
                vkGetPhysicalDeviceProperties(physical_device,&properties);
            }
        }

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
        if(version_major > 1 || version_minor >= 1)
        {
            VkPhysicalDeviceMeshShaderPropertiesEXT mesh_props_only{};
            mesh_props_only.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext=&mesh_props_only;

            vkGetPhysicalDeviceProperties2(physical_device,&properties2);
            mesh_shader_properties=mesh_props_only;
        }
        else
        {
            auto func=(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(inst,"vkGetPhysicalDeviceProperties2KHR");

            if(func)
            {
                VkPhysicalDeviceMeshShaderPropertiesEXT mesh_props_only{};
                mesh_props_only.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

                VkPhysicalDeviceProperties2 properties2{};
                properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties2.pNext=&mesh_props_only;

                func(physical_device,&properties2);
                mesh_shader_properties=mesh_props_only;
            }
        }
#endif
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

        layer_properties.resize(property_count);
        vkEnumerateDeviceLayerProperties(physical_device,&property_count,layer_properties.data());

        // Log Vulkan API version support
        GLogInfo("%s supported Vulkan API version: %d.%d",
                 debug_front.c_str(), version_major, version_minor);

        debug_out(debug_front.c_str(),layer_properties);
    }

    {
        uint32_t exten_count;

        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,nullptr);

        extension_properties.resize(exten_count);
        vkEnumerateDeviceExtensionProperties(physical_device,nullptr,&exten_count,extension_properties.data());

    #ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
        mesh_shader_extension = CheckExtensionSupport(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    #else
        mesh_shader_extension = false;
    #endif

        debug_out(debug_front.c_str(),extension_properties);
    }

#ifdef VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME
    graphics_pipeline_library = CheckExtensionSupport(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME) && graphics_pipeline_library_feature;
#else
    graphics_pipeline_library = false;
#endif

    {
        uint32_t family_count;

        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);

        queue_family_properties.resize(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,queue_family_properties.data());

        debug_queue_family_properties_out(debug_front.c_str(),queue_family_properties);
    }

    if(features14.indexTypeUint8)
        support_u8_index=true;
    else
        support_u8_index=CheckExtensionSupport(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);

    dynamic_state=CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    const char *gpl_extension_support = "no-sdk";

#ifdef VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME
    gpl_extension_support = CheckExtensionSupport(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME)?"yes":"no";
#endif

    GLogInfo("%s graphics pipeline library support: extension=%s feature=%s enabled=%s",
             debug_front.c_str(),
             gpl_extension_support,
             graphics_pipeline_library_feature?"yes":"no",
             graphics_pipeline_library?"yes":"no");

#ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
    const char *mesh_ext_support = mesh_shader_extension ? "yes" : "no";
#else
    const char *mesh_ext_support = "no-sdk";
#endif

    GLogInfo("%s mesh shader support: extension=%s (runtime mesh/task enablement is validated during device creation)",
             debug_front.c_str(),
             mesh_ext_support);

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
