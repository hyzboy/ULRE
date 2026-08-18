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
#include<hgl/vk/VKPipelineConfig.h>
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
                            const VulkanHardwareRequirement &require,
                            const bool enable_graphics_pipeline_library)
    {
        ext_list->Add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        constexpr const char *require_ext_list[]=
        {
        #ifdef _DEBUG
            VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
        #endif//_DEBUG
            VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
//            VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
            VK_EXT_PRIMITIVE_TOPOLOGY_LIST_RESTART_EXTENSION_NAME,
//            VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME,
//            VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
//            VK_EXT_HDR_METADATA_EXTENSION_NAME,
//            VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME,
//            VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME,
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,

//            VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,

            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        };

        for(const char *ext_name:require_ext_list)
            if(physical_device->CheckExtensionSupport(ext_name))
                ext_list->Add(ext_name);

        if(enable_graphics_pipeline_library
        && !FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY
        && physical_device->SupportGraphicsPipelineLibrary())
        {
            ext_list->Add(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
            ext_list->Add(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
        }

        if(require.lineRasterization>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

        if(require.texture_compression.PVRTC>=VulkanHardwareRequirement::SupportLevel::Want)                   //前面检测过了，所以这里不用再次检测是否支持
            ext_list->Add(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);

        if(require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
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

        REQURE_TEXTURE_FEATURE_COPY(BC);
        REQURE_TEXTURE_FEATURE_COPY(ETC2);
        REQURE_TEXTURE_FEATURE_COPY(ASTC_LDR);

        #undef REQURE_TEXTURE_FEATURE_COPY
        #undef REQURE_FEATURE_COPY
        #undef FEATURE_COPY
    }

    void GetDeviceQueue(VulkanDevAttr *attr)
    {
        vkGetDeviceQueue(attr->device,attr->surface->GetGraphicsFamilyIndex(),0,&attr->graphics_queue);

        attr->present_queue=attr->graphics_queue;
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

    VkDescriptorPool CreateDescriptorPool(VkDevice device,uint32_t sets_count)
    {
        VkDescriptorPoolSize pool_size[]=
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets_count},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          sets_count},
            {VK_DESCRIPTOR_TYPE_SAMPLER,                sets_count},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         sets_count},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, sets_count},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         sets_count},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, sets_count},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       sets_count}
        };

        VkDescriptorPoolCreateInfo dp_create_info;
        dp_create_info.sType        =VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dp_create_info.pNext        =nullptr;
        dp_create_info.flags        =VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        dp_create_info.maxSets      =sets_count;
        dp_create_info.poolSizeCount=sizeof(pool_size)/sizeof(VkDescriptorPoolSize);
        dp_create_info.pPoolSizes   =pool_size;

        VkDescriptorPool desc_pool;

        if(vkCreateDescriptorPool(device,&dp_create_info,nullptr,&desc_pool)!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return desc_pool;
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

VkDevice VulkanDeviceCreater::CreateDevice(const uint32_t graphics_family,
                                           const bool enable_graphics_pipeline_library)
{
    float queue_priorities[1]={0.0};

    VkDeviceQueueCreateInfo queue_info;
    queue_info.sType            =VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext            =nullptr;
    queue_info.queueFamilyIndex =graphics_family;
    queue_info.queueCount       =1;
    queue_info.pQueuePriorities =queue_priorities;
    queue_info.flags            =0;     //如果这里写VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT，会导致vkGetDeviceQueue调用崩溃

    VkDeviceCreateInfo create_info;

    create_info.sType                   =VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext                   =nullptr;
    create_info.flags                   =0;
    create_info.queueCreateInfoCount    =1;
    create_info.pQueueCreateInfos       =&queue_info;
    create_info.enabledExtensionCount   =ext_list.GetCount();
    create_info.ppEnabledExtensionNames =ext_list.GetData();
    create_info.enabledLayerCount       =0;
    create_info.ppEnabledLayerNames     =nullptr;
    create_info.pEnabledFeatures        =&features;

    VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_type_uint8_features;
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library_features{};

    // Vulkan 1.1: shaderDrawParameters —— SSBO 顶点输入 gl_BaseVertexARB 读取必需
    // （ShaderDrawParameters capability 由该特性启用；设备 v1.4 必支持）
    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_params_features{};
    shader_draw_params_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shader_draw_params_features.shaderDrawParameters = VK_TRUE;
    create_info.pNext = &shader_draw_params_features;

    // Vulkan 1.2 统一特性结构：descriptor indexing 与 scalarBlockLayout 均已
    // 提升并入 VkPhysicalDeviceVulkan12Features，因此必须在此单一结构中设置，
    // 不得再单独链入 VkPhysicalDeviceDescriptorIndexingFeatures
    // （否则违反 VUID-VkDeviceCreateInfo-pNext-02830）。
    // 该结构始终入链，确保即使设备不支持 scalarBlockLayout，
    // 描述符索引特性依然生效。
    VkPhysicalDeviceVulkan12Features vk12_features{};
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

        create_info.pNext = &vk12_features;
    }


    if(enable_graphics_pipeline_library
    && !FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY
    && physical_device->SupportGraphicsPipelineLibrary())
    {
        graphics_pipeline_library_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
        graphics_pipeline_library_features.pNext=const_cast<void*>(static_cast<const void*>(create_info.pNext));
        graphics_pipeline_library_features.graphicsPipelineLibrary=VK_TRUE;
        create_info.pNext=&graphics_pipeline_library_features;
    }
    if(physical_device->SupportU8Index()
     &&require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
    {
        index_type_uint8_features.sType         =VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
        index_type_uint8_features.pNext         =const_cast<void*>(static_cast<const void*>(create_info.pNext));
        index_type_uint8_features.indexTypeUint8=VK_TRUE;

        create_info.pNext=&index_type_uint8_features;
    }

    VkDevice device;

    if(physical_device->CreateDevice(&create_info,&device)==VK_SUCCESS)
        return device;

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

    bool try_graphics_pipeline_library = !FORCE_DISABLE_GRAPHICS_PIPELINE_LIBRARY
                                       && physical_device->SupportGraphicsPipelineLibrary();

    SetDeviceExtension(&ext_list,physical_device,require,try_graphics_pipeline_library);
    SetDeviceFeatures(&features,physical_device->GetFeatures10(),require);

    device_attr->device=CreateDevice(graphics_family, try_graphics_pipeline_library);

    if(!device_attr->device && try_graphics_pipeline_library)
    {
        GLogWarning("[VulkanDeviceCreater] GPL device creation failed, retrying without GPL extensions");
        ext_list.Clear();
        SetDeviceExtension(&ext_list,physical_device,require,false);
        device_attr->device=CreateDevice(graphics_family, false);
        try_graphics_pipeline_library = false;
    }

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

    device_attr->graphics_pipeline_library = try_graphics_pipeline_library
                                            && device_attr->device != VK_NULL_HANDLE;

    device_attr->surface_format=surface_format;

    GetDeviceQueue(device_attr);

    device_attr->cmd_pool=CreateCommandPool(device_attr->device,graphics_family);

    if(!device_attr->cmd_pool)
        return(nullptr);

    device_attr->desc_pool=CreateDescriptorPool(device_attr->device,require.descriptor_pool);

    if(!device_attr->desc_pool)
        return(nullptr);

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
            device_attr->debug_utils->SetDescriptorPool(device_attr->desc_pool,"Main Descriptor Pool");
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
    #define VHRC_F13(name) VHRC(name,features13.name)
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

    VHRC_PDE(fullDrawIndexUint8,      EXT_INDEX_TYPE_UINT8);
    VHRC_F10(fullDrawIndexUint32);

    VHRC_TC10(BC);
    VHRC_TC10(ETC2);
    VHRC_TC10(ASTC_LDR);
    VHRC_TC13(ASTC_HDR);
    VHRC_PDE(texture_compression.PVRTC,     IMG_FORMAT_PVRTC);

    VHRC_F13(dynamicRendering);

    VHRC_PDE(dynamicState[0],      EXT_EXTENDED_DYNAMIC_STATE);
    VHRC_PDE(dynamicState[1],      EXT_EXTENDED_DYNAMIC_STATE_2);
    VHRC_PDE(dynamicState[2],      EXT_EXTENDED_DYNAMIC_STATE_3);

#undef VHRC_PDE
#undef VHRC_F13
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

        // bindless 集使用 UPDATE_AFTER_BIND 池，普通与 update-after-bind 两类上限均须满足
        const VkPhysicalDeviceVulkan12Properties &props12 = physical_device->GetProperties12();

        if(limits.maxDescriptorSetSampledImages        < BindlessTextureManager::kMax
        || props12.maxDescriptorSetUpdateAfterBindSampledImages < BindlessTextureManager::kMax
        || limits.maxDescriptorSetSamplers             < BindlessTextureManager::kMaxSampler
        || props12.maxDescriptorSetUpdateAfterBindSamplers      < BindlessTextureManager::kMaxSampler)
        {
            GLogError(u8"[VulkanDeviceCreater] 物理设备描述符集上限不足（bindless 硬需求）: "
                        u8"需要 SampledImage=%u / Sampler=%u，实际 %u/%u %u/%u",
                BindlessTextureManager::kMax,
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
