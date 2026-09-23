#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKInstance.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKDeviceCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSurface.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/mtl/ShaderCompilerProfileAPI.h>

#include<hgl/log/Log.h>

namespace hgl::graph{
VkPipelineCache CreatePipelineCache(VkDevice device,const VkPhysicalDeviceProperties &);

#ifdef _DEBUG
DebugUtils *CreateDebugUtils(VkDevice);

void LogSurfaceFormat(const VkSurfaceFormatKHR &sf)
{
    const VulkanFormat *    vf=GetVulkanFormat(sf.format);
    const VulkanColorSpace *cs=GetVulkanColorSpace(sf.colorSpace);

    GLogDebug("%-10s, %s", vf->name, cs->name);
}

void LogSurfaceFormat(const VkSurfaceFormatList &surface_formats_list)
{
    GLogDebug("Current physics device support %u surface format", surface_formats_list.GetCount());

    for(auto &sf:surface_formats_list)
        LogSurfaceFormat(sf);
}
#endif//_DEBUG

namespace
{
    void SetDeviceExtension(CharPointerList *ext_list,
                            const VulkanPhyDevice *physical_device,
                            const VulkanHardwareRequirement &require)
    {
        ext_list->Add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        constexpr const char *require_ext_list[]=
        {
        #ifdef _DEBUG
            VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
        #endif//_DEBUG

            // Vulkan 1.3/1.4 核心扩展：不再探测，直接启用
            // （extended_dynamic_state 1/2 = 1.3 核心，3 = 1.4 核心；
            //   dynamic_rendering = 1.3 核心；SPIRV_1_4/8bit/16bit/scalar_block_layout = 1.4/1.2 核心）
            VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
            VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,

            // SSBO 8/16 位数据（Luminance/Color 压缩格式直读——uint8/uint16 数组）
            // 特性在 VkPhysicalDeviceVulkan12Features 中启用（Vulkan 1.2 提升）
            VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
            VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
            VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,

            // Mesh Shader（EXT 扩展，1.4 仍非核心——但目标设备 AMD 6700 XT 已实测支持；
            // 特性在 VkPhysicalDeviceMeshShaderFeaturesEXT 中启用）
            VK_EXT_MESH_SHADER_EXTENSION_NAME,
        };

        // 1.4 硬性：以上均为核心扩展，无条件启用（vulkan1.4.md 第 2 项）
        for(const char *ext_name:require_ext_list)
            ext_list->Add(ext_name);

        if(require.lineRasterization>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

        if(require.texture_compression.PVRTC>=VulkanHardwareRequirement::SupportLevel::Want)                   //前面检测过了，所以这里不用再次检测是否支持
            ext_list->Add(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);

        if(physical_device->SupportDescriptorBuffer())
            ext_list->Add(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);

        // indexTypeUint8 走 VkPhysicalDeviceVulkan14Features（1.4 核心），无需 EXT_INDEX_TYPE_UINT8 扩展
    }

    void SetDeviceFeatures(VkPhysicalDeviceFeatures *features,const VkPhysicalDeviceFeatures &pdf,const VulkanHardwareRequirement &require)
    {
        #define FEATURE_COPY(name)  features->name=pdf.name;
        #define REQURE_FEATURE_COPY(name) if(require.name>=VulkanHardwareRequirement::SupportLevel::Want)features->name=pdf.name;
        #define REQURE_TEXTURE_FEATURE_COPY(name) if(require.texture_compression.name>=VulkanHardwareRequirement::SupportLevel::Want)features->textureCompression##name=pdf.textureCompression##name;

        FEATURE_COPY(multiDrawIndirect);
        FEATURE_COPY(samplerAnisotropy);

        REQURE_FEATURE_COPY(geometryShader);

        REQURE_FEATURE_COPY(imageCubeArray);

        REQURE_FEATURE_COPY(fullDrawIndexUint32);
        REQURE_FEATURE_COPY(sampleRateShading);

        REQURE_FEATURE_COPY(fillModeNonSolid);

        REQURE_FEATURE_COPY(wideLines)
        REQURE_FEATURE_COPY(largePoints)

        REQURE_FEATURE_COPY(shaderInt64)

        REQURE_TEXTURE_FEATURE_COPY(BC);
        REQURE_TEXTURE_FEATURE_COPY(ETC2);
        REQURE_TEXTURE_FEATURE_COPY(ASTC_LDR);

        #undef REQURE_TEXTURE_FEATURE_COPY
        #undef REQURE_FEATURE_COPY
        #undef FEATURE_COPY
    }

    void GetDeviceQueue(VulkanDevAttr *attr)
    {
        vkGetDeviceQueue(attr->device,attr->graphics_family_index,0,&attr->graphics_queue);

        attr->present_queue=attr->graphics_queue;

        if(attr->transfer_family_index != attr->graphics_family_index)
        {
            vkGetDeviceQueue(attr->device,attr->transfer_family_index,0,&attr->transfer_queue);
        }
        else
        {
            attr->transfer_queue = attr->graphics_queue;
        }
    }

    VkCommandPool CreateCommandPool(VkDevice device,uint32_t graphics_family)
    {
        VkCommandPoolCreateInfo cmd_pool_info={};

        cmd_pool_info.sType             =VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.pNext             =nullptr;
        cmd_pool_info.queueFamilyIndex  =graphics_family;
        cmd_pool_info.flags             =VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool cmd_pool;

        if(vkCreateCommandPool(device,&cmd_pool_info,nullptr,&cmd_pool)==VK_SUCCESS)
        {
            VulkanDevice *owner = VulkanDevice::FromDevice(device);
            if (owner)
                owner->TrackObject(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)(uintptr_t)cmd_pool, "CommandPool");
            return cmd_pool;
        }

        return(VK_NULL_HANDLE);
    }

    ImageView *Create2DImageView(VkDevice device,VkFormat format,const VkExtent2D &ext,const uint32_t miplevel,VkImage img=VK_NULL_HANDLE)
    {
        VkExtent3D extent;

        copy(extent,ext);

        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,extent,miplevel,VK_IMAGE_ASPECT_COLOR_BIT,img);
    }

    ImageView *CreateDepthImageView(VkDevice device,VkFormat format,const VkExtent2D &ext,const uint32_t miplevel,VkImage img=VK_NULL_HANDLE)
    {
        VkExtent3D extent;

        copy(extent,ext,1);
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,extent,miplevel,VK_IMAGE_ASPECT_DEPTH_BIT,img);
    }

    void LogDeviceCreateInfo(const VkDeviceCreateInfo *create_info, const VkResult result)
    {
        GLogError(u8"vkCreateDevice 失败，VkResult = %d", int(result));
        GLogError(u8"  queueCreateInfoCount = %u", create_info->queueCreateInfoCount);
        for(uint32_t i = 0; i < create_info->queueCreateInfoCount; ++i)
        {
            const auto &q = create_info->pQueueCreateInfos[i];
            GLogError(u8"    Queue[%u]: family = %u, count = %u", i, q.queueFamilyIndex, q.queueCount);
        }
        GLogError(u8"  enabledExtensionCount = %u", create_info->enabledExtensionCount);
        for(uint32_t i = 0; i < create_info->enabledExtensionCount; ++i)
        {
            GLogError(u8"    Extension[%u]: %s", i, create_info->ppEnabledExtensionNames[i]);
        }

        struct GenericVkHeader
        {
            VkStructureType sType;
            const void *pNext;
        };

        GLogError(u8"  pNext 链节点:");
        const void *curr = create_info->pNext;
        uint32_t node_index = 0;
        while(curr)
        {
            const auto *hdr = static_cast<const GenericVkHeader *>(curr);
            switch(hdr->sType)
            {
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES:
            {
                const auto *f = static_cast<const VkPhysicalDeviceShaderDrawParametersFeatures *>(curr);
                GLogError(u8"    [%u] ShaderDrawParametersFeatures (sType=%d): shaderDrawParameters=%d",
                          node_index, int(hdr->sType), int(f->shaderDrawParameters));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
            {
                const auto *f = static_cast<const VkPhysicalDeviceVulkan12Features *>(curr);
                GLogError(u8"    [%u] Vulkan12Features (sType=%d): scalarBlockLayout=%d, 8BitAccess=%d, bufferDeviceAddress=%d, drawIndirectCount=%d",
                          node_index, int(hdr->sType), int(f->scalarBlockLayout), int(f->storageBuffer8BitAccess), int(f->bufferDeviceAddress), int(f->drawIndirectCount));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES:
            {
                const auto *f = static_cast<const VkPhysicalDevice16BitStorageFeatures *>(curr);
                GLogError(u8"    [%u] 16BitStorageFeatures (sType=%d): storageBuffer16BitAccess=%d, uniformAndStorageBuffer16BitAccess=%d",
                          node_index, int(hdr->sType), int(f->storageBuffer16BitAccess), int(f->uniformAndStorageBuffer16BitAccess));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
            {
                const auto *f = static_cast<const VkPhysicalDeviceVulkan13Features *>(curr);
                GLogError(u8"    [%u] Vulkan13Features (sType=%d): dynamicRendering=%d, maintenance4=%d, synchronization2=%d",
                          node_index, int(hdr->sType), int(f->dynamicRendering), int(f->maintenance4), int(f->synchronization2));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES:
            {
                const auto *f = static_cast<const VkPhysicalDeviceVulkan14Features *>(curr);
                GLogError(u8"    [%u] Vulkan14Features (sType=%d): indexTypeUint8=%d",
                          node_index, int(hdr->sType), int(f->indexTypeUint8));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT:
            {
                const auto *f = static_cast<const VkPhysicalDeviceMeshShaderFeaturesEXT *>(curr);
                GLogError(u8"    [%u] MeshShaderFeaturesEXT (sType=%d): taskShader=%d, meshShader=%d, queries=%d",
                          node_index, int(hdr->sType), int(f->taskShader), int(f->meshShader), int(f->meshShaderQueries));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT:
            {
                const auto *f = static_cast<const VkPhysicalDeviceExtendedDynamicStateFeaturesEXT *>(curr);
                GLogError(u8"    [%u] ExtendedDynamicStateFeaturesEXT (sType=%d): extendedDynamicState=%d",
                          node_index, int(hdr->sType), int(f->extendedDynamicState));
                break;
            }
            case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT:
            {
                const auto *f = static_cast<const VkPhysicalDeviceExtendedDynamicState3FeaturesEXT *>(curr);
                GLogError(u8"    [%u] ExtendedDynamicState3FeaturesEXT (sType=%d): colorBlend=%d, equation=%d, writeMask=%d, polygonMode=%d, a2c=%d",
                          node_index, int(hdr->sType), int(f->extendedDynamicState3ColorBlendEnable), int(f->extendedDynamicState3ColorBlendEquation),
                          int(f->extendedDynamicState3ColorWriteMask), int(f->extendedDynamicState3PolygonMode), int(f->extendedDynamicState3AlphaToCoverageEnable));
                break;
            }
            default:
                GLogError(u8"    [%u] 未知 sType = %d, ptr = %p", node_index, int(hdr->sType), curr);
                break;
            }
            curr = hdr->pNext;
            ++node_index;
        }
    }
}//namespace

#ifndef VK_DRIVER_ID_BEGIN_RANGE
#define VK_DRIVER_ID_BEGIN_RANGE VK_DRIVER_ID_AMD_PROPRIETARY
#endif//VK_DRIVER_ID_BEGIN_RANGE

#ifndef VK_DRIVER_ID_END_RANGE
#define VK_DRIVER_ID_END_RANGE VK_DRIVER_ID_MESA_LLVMPIPE
#endif//VK_DRIVER_ID_END_RANGE

#ifndef VK_DRIVER_ID_RANGE_SIZE
constexpr size_t VK_DRIVER_ID_RANGE_SIZE=VK_DRIVER_ID_END_RANGE-VK_DRIVER_ID_BEGIN_RANGE+1;
#endif//VK_DRIVER_ID_RANGE_SIZE

#ifdef _DEBUG
void OutputPhysicalDeviceCaps(const VulkanPhyDevice *);
#endif//_DEBUG

VkDevice VulkanDeviceCreater::CreateDevice(const uint32_t graphics_family)
{
    const uint32_t transfer_family = physical_device->GetTransferFamilyIndex(graphics_family);

    float queue_priorities[1]={0.0f};

    VkDeviceQueueCreateInfo queue_infos[2]{};
    uint32_t queue_create_count = 1;

    queue_infos[0].sType            =VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[0].pNext            =nullptr;
    queue_infos[0].queueFamilyIndex =graphics_family;
    queue_infos[0].queueCount       =1;
    queue_infos[0].pQueuePriorities =queue_priorities;
    queue_infos[0].flags            =0;

    if(transfer_family != graphics_family)
    {
        queue_infos[1].sType            =VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[1].pNext            =nullptr;
        queue_infos[1].queueFamilyIndex =transfer_family;
        queue_infos[1].queueCount       =1;
        queue_infos[1].pQueuePriorities =queue_priorities;
        queue_infos[1].flags            =0;
        queue_create_count = 2;
    }

    VkDeviceCreateInfo create_info;

    create_info.sType                   =VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext                   =nullptr;
    create_info.flags                   =0;
    create_info.queueCreateInfoCount    =queue_create_count;
    create_info.pQueueCreateInfos       =queue_infos;
    create_info.enabledExtensionCount   =ext_list.GetCount();
    create_info.ppEnabledExtensionNames =ext_list.GetData();
    create_info.enabledLayerCount       =0;
    create_info.ppEnabledLayerNames     =nullptr;
    create_info.pEnabledFeatures        =&features;

    // 所有 pNext 特性结构体必须在函数作用域声明，确保调用 CreateDevice 时生命周期依然有效，
    // 避免局部代码块退出后因 Release 栈槽复用优化导致链表节点悬垂破坏
    VkPhysicalDeviceShaderDrawParametersFeatures        shader_draw_params_features{};
    VkPhysicalDeviceVulkan12Features                    vk12_features{};
    VkPhysicalDevice16BitStorageFeatures                storage16_features{};
    VkPhysicalDeviceVulkan13Features                    vulkan13_features{};
    VkPhysicalDeviceVulkan14Features                    vulkan14_features{};
    VkPhysicalDeviceMeshShaderFeaturesEXT               mesh_features{};
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT     eds1{};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT    eds3{};
    VkPhysicalDeviceDescriptorBufferFeaturesEXT         desc_buffer_features{};

    // Vulkan 1.1: shaderDrawParameters —— SSBO 顶点输入 gl_BaseVertexARB 读取必需
    // （ShaderDrawParameters capability 由该特性启用；设备 v1.4 必支持）
    shader_draw_params_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shader_draw_params_features.shaderDrawParameters = VK_TRUE;
    create_info.pNext = &shader_draw_params_features;

    // Vulkan 1.2 统一特性结构：descriptor indexing 与 scalarBlockLayout 均已
    // 提升并入 VkPhysicalDeviceVulkan12Features，因此必须在此单一结构中设置，
    // 不得再单独链入 VkPhysicalDeviceDescriptorIndexingFeatures
    // （否则违反 VUID-VkDeviceCreateInfo-pNext-02830）。
    // 该结构始终入链，确保即使设备不支持 scalarBlockLayout，
    // 描述符索引特性依然生效。
    {
        const VkPhysicalDeviceVulkan12Features &dev12 = physical_device->GetFeatures12();

        vk12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));

        // descriptor indexing（原 VkPhysicalDeviceDescriptorIndexingFeatures）
        vk12_features.shaderSampledImageArrayNonUniformIndexing   = dev12.shaderSampledImageArrayNonUniformIndexing;
        vk12_features.descriptorBindingPartiallyBound             = dev12.descriptorBindingPartiallyBound;
        vk12_features.runtimeDescriptorArray                      = dev12.runtimeDescriptorArray;
        vk12_features.descriptorBindingUniformBufferUpdateAfterBind = dev12.descriptorBindingUniformBufferUpdateAfterBind;
        vk12_features.descriptorBindingSampledImageUpdateAfterBind= dev12.descriptorBindingSampledImageUpdateAfterBind;
        vk12_features.descriptorBindingStorageBufferUpdateAfterBind = dev12.descriptorBindingStorageBufferUpdateAfterBind;
        vk12_features.descriptorBindingUpdateUnusedWhilePending   = dev12.descriptorBindingUpdateUnusedWhilePending;

        // GL_EXT_scalar_block_layout：ColorPalette UBO 使用 layout(scalar)
        // 使 uint[256] 紧凑打包（4 字节步长），与 C++ 端 1024 字节结构对齐。
        vk12_features.scalarBlockLayout = dev12.scalarBlockLayout;

        // VK_KHR_8bit_storage：SSBO 顶点输入支持 uint8 数据（Vulkan12Features 字段）
        vk12_features.storageBuffer8BitAccess                 = dev12.storageBuffer8BitAccess;
        vk12_features.uniformAndStorageBuffer8BitAccess       = dev12.uniformAndStorageBuffer8BitAccess;

        // buffer device address：材质数据 Arena 的 shader 侧寻址依赖
        // （GL_EXT_buffer_reference 生成 PhysicalStorageBuffer 指针）
        vk12_features.bufferDeviceAddress                     = dev12.bufferDeviceAddress;

        // drawIndirectCount：支持根据 GPU 计数缓冲发起间接绘制（vkCmdDrawMeshTasksIndirectCountEXT）
        vk12_features.drawIndirectCount                       = dev12.drawIndirectCount;

        create_info.pNext = &vk12_features;

        // VK_KHR_16bit_storage（独立结构——Vulkan12Features 不含 16bit storage 字段）
        storage16_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
        storage16_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        storage16_features.storageBuffer16BitAccess           = physical_device->GetFeatures16().storageBuffer16BitAccess;
        storage16_features.uniformAndStorageBuffer16BitAccess = physical_device->GetFeatures16().uniformAndStorageBuffer16BitAccess;
        create_info.pNext = &storage16_features;
    }

    // Vulkan 1.3 统一特性结构：
    // - dynamicRendering：vkCmdBeginRendering/EndRendering 必须启用（VUID-vkCmdBeginRendering-dynamicRendering-06446）
    // - maintenance4：mesh shader 的 SPIR-V OpExecutionMode LocalSizeId 要求启用
    //   （glslang 16 生成 SPIR-V 1.6 时使用 LocalSizeId 而非 LocalSize；VUID-RuntimeSpirv-LocalSizeId-06434）
    // - synchronization2：强制硬件要求，vkCmdPipelineBarrier2 / vkQueueSubmit2 核心路径
    {
        const VkPhysicalDeviceVulkan13Features &dev13 = physical_device->GetFeatures13();

        if(!dev13.synchronization2)
        {
            GLogError(u8"[VKDeviceCreater] 硬件不支持 synchronization2，创建设备失败！");
            return nullptr;
        }

        vulkan13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vulkan13_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        vulkan13_features.dynamicRendering = dev13.dynamicRendering;
        vulkan13_features.maintenance4     = dev13.maintenance4;
        vulkan13_features.synchronization2 = VK_TRUE;

        create_info.pNext = &vulkan13_features;
    }


    {
        const VkPhysicalDeviceVulkan14Features &dev14 = physical_device->GetFeatures14();

        // Vulkan 1.4 核心：indexTypeUint8 与 pushDescriptor 经 VkPhysicalDeviceVulkan14Features 启用
        vulkan14_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vulkan14_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));

        if(physical_device->SupportU8Index()
         &&require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
        {
            vulkan14_features.indexTypeUint8 = dev14.indexTypeUint8;
        }

        vulkan14_features.pushDescriptor = dev14.pushDescriptor;

        create_info.pNext=&vulkan14_features;
    }

    // VK_EXT_mesh_shader：设备特性启用（meshShader/taskShader）——mesh shader 为必用路径，无条件启用
    {
        const VkPhysicalDeviceMeshShaderFeaturesEXT &dev_mesh = physical_device->GetMeshShaderFeatures();

        mesh_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        mesh_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        mesh_features.taskShader    = dev_mesh.taskShader;
        mesh_features.meshShader    = dev_mesh.meshShader;
        mesh_features.meshShaderQueries = dev_mesh.meshShaderQueries;

        create_info.pNext=&mesh_features;
    }

    // EDS 1/3 特性：pipeline 只保留 shader 部分——材质渲染状态全部动态（vkCmdSet* 应用），
    // 必用路径无条件启用。EDS1 管 CULL_MODE/DEPTH_*（VkPhysicalDeviceExtendedDynamicStateFeaturesEXT）；
    // COLOR_BLEND_*/POLYGON_MODE/ALPHA_TO_COVERAGE 均属 EDS3
    //（VkPhysicalDeviceExtendedDynamicState3FeaturesEXT——SDK 1.4 头里 EDS2 结构无这些成员）
    {
        eds1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        eds1.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        eds1.extendedDynamicState = VK_TRUE;   // CULL_MODE / DEPTH_TEST / DEPTH_WRITE / DEPTH_COMPARE_OP
        create_info.pNext = &eds1;

        eds3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
        eds3.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        eds3.extendedDynamicState3ColorBlendEnable    = VK_TRUE;   // COLOR_BLEND_ENABLE
        eds3.extendedDynamicState3ColorBlendEquation  = VK_TRUE;   // COLOR_BLEND_EQUATION
        eds3.extendedDynamicState3ColorWriteMask      = VK_TRUE;   // COLOR_WRITE_MASK
        eds3.extendedDynamicState3PolygonMode         = VK_TRUE;   // POLYGON_MODE
        eds3.extendedDynamicState3AlphaToCoverageEnable = VK_TRUE; // ALPHA_TO_COVERAGE_ENABLE
        create_info.pNext = &eds3;
    }

    if(physical_device->SupportDescriptorBuffer())
    {
        desc_buffer_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
        desc_buffer_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        desc_buffer_features.descriptorBuffer = VK_TRUE;
        desc_buffer_features.descriptorBufferPushDescriptors = physical_device->GetDescriptorBufferFeatures().descriptorBufferPushDescriptors;
        create_info.pNext = &desc_buffer_features;
    }

    VkDevice device = VK_NULL_HANDLE;
    const VkResult result = physical_device->CreateDevice(&create_info,&device);

    if(result == VK_SUCCESS)
        return device;

    LogDeviceCreateInfo(&create_info, result);

    return nullptr;
}

void VulkanDeviceCreater::ChooseSurfaceFormat()
{
    const VkSurfaceFormatList &surface_formats_list=surface->GetFormats();

    if(surface_formats_list.IsEmpty())
        return;

#ifdef _DEBUG
    LogSurfaceFormat(surface_formats_list);
#endif//_DEBUG

    bool sel=false;
    {
        int fmt_index=-1;
        int cs_index=-1;
        int fmt;
        int cs;

        for(auto sf:surface_formats_list)
        {
            fmt=perfer_color_formats->Find(sf.format);
            cs=perfer_color_spaces->Find(sf.colorSpace);

            if((fmt==fmt_index&&cs>cs_index)||fmt>fmt_index)
            {
                surface_format=sf;

                fmt_index=fmt;
                cs_index=cs;
                sel=true;
            }
        }
    }

    if(!sel)
    {
        surface_format.format=PF_RGBA8s;
        surface_format.colorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }

#ifdef _DEBUG
    LogSurfaceFormat(surface_format);
#endif//_DEBUG
}

VulkanDevice *VulkanDeviceCreater::CreateRenderDevice()
{
    VulkanDevAttr *device_attr=new VulkanDevAttr(instance,physical_device,surface);

    AutoDelete<VulkanDevAttr> auto_delete(device_attr);

    const uint32_t graphics_family=device_attr->surface->GetGraphicsFamilyIndex();

    if(graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    const uint32_t transfer_family=physical_device->GetTransferFamilyIndex(graphics_family);
    device_attr->graphics_family_index = graphics_family;
    device_attr->transfer_family_index = transfer_family;

    // 管线物化路径：仅单 pipeline（GPL 已整体删除——激活实测在混合材质上存在
    // 渲染正确性问题且无法用 RenderDoc 调试，详见 2026-08-29 提交记录）。
    // 现状：pipeline 只保留 shader 部分，全部渲染状态走 EDS 1/2/3 动态设置
    //（vkCmdSet* 渲染侧应用材质配置）；远期可迁 VK_EXT_shader_object。
    SetDeviceExtension(&ext_list,physical_device,require);
    SetDeviceFeatures(&features,physical_device->GetFeatures10(),require);

    device_attr->device=CreateDevice(graphics_family);

    if(!device_attr->device)
        return(nullptr);

    ChooseSurfaceFormat();

    if(physical_device->SupportU8Index()
     &&require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
    {
        device_attr->uint8_index_type=true;
    }

    if(physical_device->SupportU32Index()
     &&require.fullDrawIndexUint32>=VulkanHardwareRequirement::SupportLevel::Want)
    {
        device_attr->uint32_index_type=true;
    }

    if(physical_device->SupportWideLines()
        || require.wideLines >= VulkanHardwareRequirement::SupportLevel::Want)
    {
        device_attr->wide_lines = true;
    }

    // VK_EXT_mesh_shader：加载扩展函数指针（仅一次——mesh shader 为必用路径）
    {
        auto func_ptr=device_attr->GetDeviceProc<PFN_vkCmdDrawMeshTasksEXT>("vkCmdDrawMeshTasksEXT");
        if(func_ptr)
            device_attr->cmd_draw_mesh_tasks=*func_ptr;

        // indirect 变体（multi-draw 合批用；不支持 MDI 时逐条退化由 RenderCmdBuffer 处理）
        auto func_ptr_indirect=device_attr->GetDeviceProc<PFN_vkCmdDrawMeshTasksIndirectEXT>("vkCmdDrawMeshTasksIndirectEXT");
        if(func_ptr_indirect)
            device_attr->cmd_draw_mesh_tasks_indirect=*func_ptr_indirect;

        // indirect count 变体（GPU-driven 动态计数绘制，vkCmdDrawMeshTasksIndirectCountEXT）
        auto func_ptr_indirect_count=device_attr->GetDeviceProc<PFN_vkCmdDrawMeshTasksIndirectCountEXT>("vkCmdDrawMeshTasksIndirectCountEXT");
        if(func_ptr_indirect_count)
            device_attr->cmd_draw_mesh_tasks_indirect_count=*func_ptr_indirect_count;
    }

    // EDS 1/2/3 动态状态函数指针（pipeline 只保留 shader 部分——渲染状态全部 vkCmdSet* 应用）
    {
        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetCullModeEXT>("vkCmdSetCullModeEXT"))
            device_attr->cmd_set_cull_mode=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetDepthTestEnableEXT>("vkCmdSetDepthTestEnableEXT"))
            device_attr->cmd_set_depth_test_enable=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetDepthWriteEnableEXT>("vkCmdSetDepthWriteEnableEXT"))
            device_attr->cmd_set_depth_write_enable=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetDepthCompareOpEXT>("vkCmdSetDepthCompareOpEXT"))
            device_attr->cmd_set_depth_compare_op=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetColorBlendEnableEXT>("vkCmdSetColorBlendEnableEXT"))
            device_attr->cmd_set_color_blend_enable=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetColorBlendEquationEXT>("vkCmdSetColorBlendEquationEXT"))
            device_attr->cmd_set_color_blend_equation=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetColorWriteMaskEXT>("vkCmdSetColorWriteMaskEXT"))
            device_attr->cmd_set_color_write_mask=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetPolygonModeEXT>("vkCmdSetPolygonModeEXT"))
            device_attr->cmd_set_polygon_mode=*fp;

        if(auto fp=device_attr->GetDeviceProc<PFN_vkCmdSetAlphaToCoverageEnableEXT>("vkCmdSetAlphaToCoverageEnableEXT"))
            device_attr->cmd_set_alpha_to_coverage_enable=*fp;
    }

    // Push Descriptor 函数指针（Vulkan 1.4 core / VK_KHR_push_descriptor）
    {
        auto fp = device_attr->GetDeviceProc<PFN_vkCmdPushDescriptorSet>("vkCmdPushDescriptorSet");
        if(!fp)
            fp = device_attr->GetDeviceProc<PFN_vkCmdPushDescriptorSet>("vkCmdPushDescriptorSetKHR");
        if(fp)
            device_attr->cmd_push_descriptor_set = *fp;
    }

    // Synchronization2 函数指针（Vulkan 1.3 core / VK_KHR_synchronization2）
    {
        auto fp_barrier = device_attr->GetDeviceProc<PFN_vkCmdPipelineBarrier2>("vkCmdPipelineBarrier2");
        if(!fp_barrier)
            fp_barrier = device_attr->GetDeviceProc<PFN_vkCmdPipelineBarrier2>("vkCmdPipelineBarrier2KHR");
        if(fp_barrier)
            device_attr->cmd_pipeline_barrier2 = *fp_barrier;

        auto fp_submit = device_attr->GetDeviceProc<PFN_vkQueueSubmit2>("vkQueueSubmit2");
        if(!fp_submit)
            fp_submit = device_attr->GetDeviceProc<PFN_vkQueueSubmit2>("vkQueueSubmit2KHR");
        if(fp_submit)
            device_attr->queue_submit2 = *fp_submit;

        if(!device_attr->cmd_pipeline_barrier2 || !device_attr->queue_submit2)
        {
            GLogError(u8"[VKDeviceCreater] 无法获取 vkCmdPipelineBarrier2 或 vkQueueSubmit2 函数指针！");
            return nullptr;
        }
    }

    // Descriptor Buffer 函数指针（VK_EXT_descriptor_buffer）
    if(physical_device->SupportDescriptorBuffer())
    {
        device_attr->use_descriptor_buffer = true;

        if(auto fp = device_attr->GetDeviceProc<PFN_vkGetDescriptorSetLayoutSizeEXT>("vkGetDescriptorSetLayoutSizeEXT"))
            device_attr->get_descriptor_set_layout_size = *fp;

        if(auto fp = device_attr->GetDeviceProc<PFN_vkGetDescriptorSetLayoutBindingOffsetEXT>("vkGetDescriptorSetLayoutBindingOffsetEXT"))
            device_attr->get_descriptor_set_layout_binding_offset = *fp;

        if(auto fp = device_attr->GetDeviceProc<PFN_vkGetDescriptorEXT>("vkGetDescriptorEXT"))
            device_attr->get_descriptor = *fp;

        if(auto fp = device_attr->GetDeviceProc<PFN_vkCmdBindDescriptorBuffersEXT>("vkCmdBindDescriptorBuffersEXT"))
            device_attr->cmd_bind_descriptor_buffers = *fp;

        if(auto fp = device_attr->GetDeviceProc<PFN_vkCmdSetDescriptorBufferOffsetsEXT>("vkCmdSetDescriptorBufferOffsetsEXT"))
            device_attr->cmd_set_descriptor_buffer_offsets = *fp;
    }

    device_attr->surface_format=surface_format;

    GetDeviceQueue(device_attr);

    device_attr->cmd_pool=CreateCommandPool(device_attr->device,graphics_family);

    if(!device_attr->cmd_pool)
        return(nullptr);

    if(transfer_family != graphics_family)
    {
        device_attr->transfer_cmd_pool=CreateCommandPool(device_attr->device,transfer_family);
        if(!device_attr->transfer_cmd_pool)
            return(nullptr);
    }
    else
    {
        device_attr->transfer_cmd_pool=device_attr->cmd_pool;
    }

    device_attr->pipeline_cache=CreatePipelineCache(device_attr->device,physical_device->GetProperties());

    if(!device_attr->pipeline_cache)
        return(nullptr);

    auto_delete.Discard();  //discard autodelete

    #ifdef _DEBUG
        device_attr->debug_utils=CreateDebugUtils(device_attr->device);

        if(device_attr->debug_utils)
        {
            device_attr->debug_utils->SetPhysicalDevice(physical_device->GetVulkanDevice(),"Physical Device:"+AnsiString(physical_device->GetDeviceName()));
            device_attr->debug_utils->SetDevice(device_attr->device,"Device:"+AnsiString(physical_device->GetDeviceName()));
            device_attr->debug_utils->SetSurfaceKHR(surface->GetSurface(),"Surface");
            device_attr->debug_utils->SetCommandPool(device_attr->cmd_pool,"Main Command Pool");
            if(device_attr->transfer_cmd_pool && device_attr->transfer_cmd_pool != device_attr->cmd_pool)
                device_attr->debug_utils->SetCommandPool(device_attr->transfer_cmd_pool,"Transfer Command Pool");
            device_attr->debug_utils->SetPipelineCache(device_attr->pipeline_cache,"Main Pipeline Cache");
        }
    #endif//_DEBUG

    return(new VulkanDevice(device_attr));
}

VulkanDeviceCreater::VulkanDeviceCreater(   VulkanInstance *vi,
                                            Window *win,
                                            const VulkanHardwareRequirement *req,
                                            const PreferFormats *spf_color,
                                            const PreferColorSpaces *spf_color_space,
                                            const PreferFormats *spf_depth)
{
    instance=vi;
    window=win;

    physical_device=nullptr;

    perfer_color_formats=spf_color;
    perfer_color_spaces =spf_color_space;
    perfer_depth_formats=spf_depth;

    if(req)
        mem_copy(require,*req);
}

bool VulkanDeviceCreater::ChoosePhysicalDevice()
{
    physical_device=nullptr;

    if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
    if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
    if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡

    return physical_device;
}

bool VulkanDeviceCreater::RequirementCheck()
{
    const VkPhysicalDeviceLimits &limits=physical_device->GetLimits();

#define VHR_MINCHECK(name) if(require.name>0&&require.name>limits.name)return(false);

    VHR_MINCHECK(maxImageDimension1D     )
    VHR_MINCHECK(maxImageDimension2D     )
    VHR_MINCHECK(maxImageDimension3D     )
    VHR_MINCHECK(maxImageDimensionCube   )
    VHR_MINCHECK(maxImageArrayLayers     )

    VHR_MINCHECK(maxVertexInputAttributes)
    VHR_MINCHECK(maxColorAttachments     )

    VHR_MINCHECK(maxPushConstantsSize    )
    VHR_MINCHECK(maxUniformBufferRange   )
    VHR_MINCHECK(maxStorageBufferRange   )

    VHR_MINCHECK(maxDrawIndirectCount    )

#undef VHR_MINCHECK

    const VkPhysicalDeviceFeatures &features10=physical_device->GetFeatures10();
    const VkPhysicalDeviceVulkan13Features &features13=physical_device->GetFeatures13();

#define VHRC(name,check) if(require.name>=VulkanHardwareRequirement::SupportLevel::Must&&(!check))return(false);

    #define VHRC_F10(name) VHRC(name,features10.name)
    #define VHRC_PDE(name,pdename) VHRC(name,physical_device->CheckExtensionSupport(VK_##pdename##_EXTENSION_NAME))
    #define VHRC_TC10(name) VHRC(texture_compression.name,features10.textureCompression##name)
    #define VHRC_TC13(name) VHRC(texture_compression.name,features13.textureCompression##name)

    VHRC_F10(geometryShader);
    VHRC_F10(tessellationShader);

    VHRC_F10(multiDrawIndirect);

    VHRC_F10(sampleRateShading);

    VHRC_F10(fillModeNonSolid);

    VHRC_F10(wideLines);

#ifndef __APPLE__
    VHRC_PDE(lineRasterization,    EXT_LINE_RASTERIZATION);
#endif//__APPLE__

    VHRC_F10(largePoints);

    VHRC_F10(imageCubeArray);

    // Vulkan 1.4 硬性：indexTypeUint8 为核心特性（VkPhysicalDeviceVulkan14Features）
    VHRC(fullDrawIndexUint8, physical_device->GetFeatures14().indexTypeUint8);
    VHRC_F10(fullDrawIndexUint32);

    VHRC_TC10(BC);
    VHRC_TC10(ETC2);
    VHRC_TC10(ASTC_LDR);
    VHRC_TC13(ASTC_HDR);
    VHRC_PDE(texture_compression.PVRTC,     IMG_FORMAT_PVRTC);

    // Vulkan 1.3/1.4 硬性：dynamicRendering / extended_dynamic_state 1/2/3 均为核心，
    // 无需再按扩展探测（vulkan1.4.md 第 2 项）

#undef VHRC_PDE
#undef VHRC_F10
#undef VHRC

    // ── bindless 纹理架构硬需求（descriptor indexing）────────────────────
    // 现代 Vulkan 1.2+ 设备均支持；不支持即无法运行，直接报错退出。
    {
        const VkPhysicalDeviceVulkan12Features &features12 = physical_device->GetFeatures12();

        // 注意：不存在 shaderSamplerArrayNonUniformIndexing 特性，
        // 采样器数组非均匀访问由 ShaderNonUniform capability 覆盖
        // （随 shaderSampledImageArrayNonUniformIndexing 一并启用）。
        if(!features12.descriptorIndexing
        || !features12.shaderSampledImageArrayNonUniformIndexing
        || !features12.descriptorBindingPartiallyBound
        || !features12.descriptorBindingSampledImageUpdateAfterBind
        || !features12.runtimeDescriptorArray)
        {
            GLogError(u8"[VulkanDeviceCreater] 物理设备不支持 descriptor indexing（bindless 硬需求）: "
                        u8"descriptorIndexing=%d shaderSampledImageArrayNonUniformIndexing=%d "
                        u8"descriptorBindingPartiallyBound=%d descriptorBindingSampledImageUpdateAfterBind=%d runtimeDescriptorArray=%d",
                features12.descriptorIndexing,
                features12.shaderSampledImageArrayNonUniformIndexing,
                features12.descriptorBindingPartiallyBound,
                features12.descriptorBindingSampledImageUpdateAfterBind,
                features12.runtimeDescriptorArray);
            return(false);
        }

        // ── 材质数据 Arena+BDA 硬需求（buffer device address + shaderInt64）──
        // GL_EXT_buffer_reference 生成的 PhysicalStorageBuffer 指针依赖两者；
        // 与 descriptor indexing 同级：不支持即无法运行，直接报错退出。
        if(!features12.bufferDeviceAddress
        || !features10.shaderInt64)
        {
            GLogError(u8"[VulkanDeviceCreater] 物理设备不支持 bufferDeviceAddress/shaderInt64（材质数据 Arena 硬需求）: "
                        u8"bufferDeviceAddress=%d shaderInt64=%d",
                features12.bufferDeviceAddress,
                features10.shaderInt64);
            return(false);
        }

        // bindless 集使用 UPDATE_AFTER_BIND 池，普通与 update-after-bind 两类上限均须满足
        // binding=0(texture2DArray) 与 binding=2(textureCube) 各占 kMax 个，总计 2 * kMax
        constexpr uint32_t kRequiredSampledImages = BindlessTextureManager::kMax * 2;
        const VkPhysicalDeviceVulkan12Properties &props12 = physical_device->GetProperties12();

        if(limits.maxDescriptorSetSampledImages        < kRequiredSampledImages
        || props12.maxDescriptorSetUpdateAfterBindSampledImages < kRequiredSampledImages
        || limits.maxDescriptorSetSamplers             < BindlessTextureManager::kMaxSampler
        || props12.maxDescriptorSetUpdateAfterBindSamplers      < BindlessTextureManager::kMaxSampler)
        {
            GLogError(u8"[VulkanDeviceCreater] 物理设备描述符集上限不足（bindless 硬需求）: "
                        u8"需要 SampledImage=%u / Sampler=%u，实际 %u/%u %u/%u",
                kRequiredSampledImages,
                BindlessTextureManager::kMaxSampler,
                limits.maxDescriptorSetSampledImages,          props12.maxDescriptorSetUpdateAfterBindSampledImages,
                limits.maxDescriptorSetSamplers,               props12.maxDescriptorSetUpdateAfterBindSamplers);
            return(false);
        }
    }

    return(true);
}

VulkanDevice *VulkanDeviceCreater::Create()
{
    if(!instance||!window)
        return(nullptr);

    if(!ChoosePhysicalDevice())
        return(nullptr);

    #ifdef _DEBUG
        OutputPhysicalDeviceCaps(physical_device);
    #endif//_DEBUG

    mtl::SetShaderCompilerPhysicalDeviceProfile(physical_device->GetPhysicalDeviceProfile());

    if(!RequirementCheck())
        return(nullptr);

    VkSurfaceKHR vk_surface=CreateVulkanSurface(instance->GetVulkanInstance(),window);

    if(!vk_surface)
        return(nullptr);

    surface=new VulkanSurface(physical_device,vk_surface);

    extent.width    =window->GetWidth();
    extent.height   =window->GetHeight();

    VulkanDevice *device=CreateRenderDevice();

    if(!device)
    {
        delete surface;
        surface=nullptr;
        return(nullptr);
    }

    return device;
}
}//namespace hgl::graph
