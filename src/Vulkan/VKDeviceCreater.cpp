#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKInstance.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKDeviceCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSurface.h>
#include<hgl/shadergen/GLSLCompilerConfig.h>
#include<vk_mem_alloc.h>

#include<hgl/log/Log.h>

#include<cstring>

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
    GLogDebug("Current physics device support %u surface format", (uint32_t)surface_formats_list.size());

    for(auto &sf:surface_formats_list)
        LogSurfaceFormat(sf);
}
#endif//_DEBUG

namespace
{
    constexpr const char *BoolText(const bool v)
    {
        return v ? "yes" : "no";
    }

    bool HasDeviceExtension(const CharPointerList &ext_list,const char *ext_name)
    {
        if(!ext_name || !*ext_name)
            return false;

        const int count=ext_list.GetCount();

        for(int i=0;i<count;i++)
        {
            const char *name=nullptr;

            if(!ext_list.Get(i,name))
                continue;

            if(name&&std::strcmp(name,ext_name)==0)
                return true;
        }

        return false;
    }

    void AddDeviceExtensionIfSupported(CharPointerList *ext_list,const VulkanPhyDevice *physical_device,const char *ext_name)
    {
        if(!ext_list||!physical_device||!ext_name||!*ext_name)
            return;

        if(!physical_device->CheckExtensionSupport(ext_name))
            return;

        if(HasDeviceExtension(*ext_list,ext_name))
            return;

        ext_list->Add(ext_name);
    }

    void SetDeviceExtension(CharPointerList *ext_list,const VulkanPhyDevice *physical_device,const VulkanHardwareRequirement &require)
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

#ifdef VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME
        if(physical_device->CheckExtensionSupport(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME))
            ext_list->Add(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
#endif

#ifdef VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME
        if(physical_device->SupportGraphicsPipelineLibrary())
            ext_list->Add(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
#endif

        if(require.lineRasterization>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

        if(require.texture_compression.PVRTC>=VulkanHardwareRequirement::SupportLevel::Want)                   //前面检测过了，所以这里不用再次检测是否支持
            ext_list->Add(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);

        if(require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);

    #ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
        if(require.meshShaderOnlyMode
        || require.meshShader>=VulkanHardwareRequirement::SupportLevel::Want
        || require.taskShader>=VulkanHardwareRequirement::SupportLevel::Want)
        {
            AddDeviceExtensionIfSupported(ext_list,physical_device,VK_EXT_MESH_SHADER_EXTENSION_NAME);
        }
    #endif
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

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    // Probe mesh/task feature bits directly from Vulkan at the call site to avoid
    // stale/chain-dependent cached values during device bring-up.
    void ProbeMeshShaderFeatureBits(const VulkanPhyDevice *physical_device,bool &mesh_supported,bool &task_supported)
    {
        mesh_supported=false;
        task_supported=false;

        if(!physical_device)
            return;

#ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
        if(!physical_device->CheckExtensionSupport(VK_EXT_MESH_SHADER_EXTENSION_NAME))
            return;
#endif

        const VkInstance instance=physical_device->GetVulkanInstance();
        const VkPhysicalDevice vk_physical_device=physical_device->GetVulkanDevice();

        auto merge_probe=[&](const VkPhysicalDeviceMeshShaderFeaturesEXT &probe)
        {
            if(probe.meshShader==VK_TRUE)
                mesh_supported=true;

            if(probe.taskShader==VK_TRUE)
                task_supported=true;
        };

        bool queried=false;

        if(instance)
        {
            if(auto core_query=(PFN_vkGetPhysicalDeviceFeatures2)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2"))
            {
                VkPhysicalDeviceMeshShaderFeaturesEXT probe{};
                probe.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

                VkPhysicalDeviceFeatures2 features2{};
                features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                features2.pNext=&probe;

                core_query(vk_physical_device,&features2);
                merge_probe(probe);
                queried=true;
            }

            if(auto khr_query=(PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2KHR"))
            {
                VkPhysicalDeviceMeshShaderFeaturesEXT probe{};
                probe.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

                VkPhysicalDeviceFeatures2 features2{};
                features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                features2.pNext=&probe;

                khr_query(vk_physical_device,&features2);
                merge_probe(probe);
                queried=true;
            }
        }

        if(!queried)
        {
            VkPhysicalDeviceMeshShaderFeaturesEXT probe{};
            probe.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext=&probe;

            vkGetPhysicalDeviceFeatures2(vk_physical_device,&features2);
            merge_probe(probe);
        }
    }
#endif

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
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         sets_count},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, sets_count},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         sets_count},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, sets_count},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       sets_count}
        };

        VkDescriptorPoolCreateInfo dp_create_info;
        dp_create_info.sType        =VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dp_create_info.pNext        =nullptr;
        dp_create_info.flags        =0;
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

VkDevice VulkanDeviceCreater::CreateDevice(const uint32_t graphics_family)
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
    VkPhysicalDeviceVulkan12Features features12{};
    const bool enable_mesh_shader_ext=
#ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
        HasDeviceExtension(ext_list,VK_EXT_MESH_SHADER_EXTENSION_NAME);
#else
        false;
#endif

    bool mesh_shader_runtime_supported=physical_device->SupportMeshShader();
    bool task_shader_runtime_supported=physical_device->SupportTaskShader();

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    ProbeMeshShaderFeatureBits(physical_device,mesh_shader_runtime_supported,task_shader_runtime_supported);
#endif

    if(require.meshShaderOnlyMode&&enable_mesh_shader_ext)
    {
        // In mesh-only policy, try enabling task+mesh at device-create time even
        // when feature probes are ambiguous under some layer stacks.
        mesh_shader_runtime_supported=true;
        task_shader_runtime_supported=true;
    }

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features{};
#endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
    VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT graphics_pipeline_library_features{};
#endif

    if (!physical_device->GetFeatures12().scalarBlockLayout)
    {
        std::fprintf(stderr,
            "[VulkanDeviceCreater] CreateDevice failed: scalarBlockLayout is required but not supported by physical device '%s'\n",
            physical_device->GetDeviceName());
        return nullptr;
    }


    if(physical_device->SupportU8Index()
     &&require.fullDrawIndexUint8>=VulkanHardwareRequirement::SupportLevel::Want)
    {
        index_type_uint8_features.sType         =VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT;
        index_type_uint8_features.pNext         =const_cast<void*>(static_cast<const void*>(create_info.pNext));
        index_type_uint8_features.indexTypeUint8=VK_TRUE;

        create_info.pNext=&index_type_uint8_features;
    }

    // Vulkan 1.3 baseline in this project: force Vulkan 1.2 scalarBlockLayout so
    // all MaterialBindingInstance SSBOs can use layout(scalar) consistently.
    // 启用 descriptorBindingPartiallyBound (promoted to Vulkan 1.2 core; must not use VkPhysicalDeviceDescriptorIndexingFeatures alongside VkPhysicalDeviceVulkan12Features)
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
    features12.scalarBlockLayout = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    create_info.pNext = &features12;

    // 启用 Vulkan 1.3 dynamicRendering（vkCmdBeginRendering / vkCmdEndRendering）
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext            = const_cast<void*>(static_cast<const void*>(create_info.pNext));
    features13.dynamicRendering = VK_TRUE;
    features13.maintenance4     = physical_device->GetFeatures13().maintenance4;
    features13.shaderDemoteToHelperInvocation = physical_device->GetFeatures13().shaderDemoteToHelperInvocation;
    create_info.pNext           = &features13;

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    if(enable_mesh_shader_ext)
    {
        mesh_shader_features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
        mesh_shader_features.pNext=const_cast<void*>(static_cast<const void*>(create_info.pNext));
        mesh_shader_features.taskShader=task_shader_runtime_supported?VK_TRUE:VK_FALSE;
        mesh_shader_features.meshShader=mesh_shader_runtime_supported?VK_TRUE:VK_FALSE;
        mesh_shader_features.multiviewMeshShader=physical_device->GetMeshShaderFeatures().multiviewMeshShader;
        mesh_shader_features.primitiveFragmentShadingRateMeshShader=physical_device->GetMeshShaderFeatures().primitiveFragmentShadingRateMeshShader;
        mesh_shader_features.meshShaderQueries=physical_device->GetMeshShaderFeatures().meshShaderQueries;
        create_info.pNext=&mesh_shader_features;
    }
#endif

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT
    if(physical_device->SupportGraphicsPipelineLibrary())
    {
        graphics_pipeline_library_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GRAPHICS_PIPELINE_LIBRARY_FEATURES_EXT;
        graphics_pipeline_library_features.pNext = const_cast<void*>(static_cast<const void*>(create_info.pNext));
        graphics_pipeline_library_features.graphicsPipelineLibrary = VK_TRUE;
        create_info.pNext = &graphics_pipeline_library_features;
    }
#endif

    VkDevice device;

    if(physical_device->CreateDevice(&create_info,&device)==VK_SUCCESS)
        return device;

    return nullptr;
}

void VulkanDeviceCreater::ChooseSurfaceFormat()
{
    const VkSurfaceFormatList &surface_formats_list=surface->GetFormats();

    if(surface_formats_list.empty())
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

    SetDeviceExtension(&ext_list,physical_device,require);
    SetDeviceFeatures(&features,physical_device->GetFeatures10(),require);

    const bool mesh_shader_ext_supported=physical_device->SupportMeshShaderExtension();
    bool mesh_shader_supported=physical_device->SupportMeshShader();
    bool task_shader_supported=physical_device->SupportTaskShader();

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    ProbeMeshShaderFeatureBits(physical_device,mesh_shader_supported,task_shader_supported);
#endif

#ifdef VK_EXT_MESH_SHADER_EXTENSION_NAME
    const bool mesh_shader_ext_enabled=HasDeviceExtension(ext_list,VK_EXT_MESH_SHADER_EXTENSION_NAME);
#else
    const bool mesh_shader_ext_enabled=false;
#endif

    const bool optimistic_mesh_only_enable=require.meshShaderOnlyMode
                                         &&mesh_shader_ext_enabled
                                         &&(!mesh_shader_supported||!task_shader_supported);

    if(optimistic_mesh_only_enable)
    {
        GLogInfo("[VulkanDeviceCreater] MeshShaderOnly mode: feature probe reports mesh=%s task=%s; attempting definitive enable at vkCreateDevice.",
                 BoolText(mesh_shader_supported),
                 BoolText(task_shader_supported));
    }

        GLogInfo("[VulkanDeviceCreater] MeshShader support matrix: extension=%s mesh=%s task=%s",
            BoolText(mesh_shader_ext_supported),
            BoolText(mesh_shader_supported),
            BoolText(task_shader_supported));

        GLogInfo("[VulkanDeviceCreater] MeshShader request: onlyMode=%s require.mesh=%d require.task=%d extensionEnabled=%s",
            BoolText(require.meshShaderOnlyMode),
            int(require.meshShader),
            int(require.taskShader),
            BoolText(mesh_shader_ext_enabled));

    device_attr->device=CreateDevice(graphics_family);

    if(!device_attr->device)
        return(nullptr);

    device_attr->mesh_shader_extension=mesh_shader_ext_enabled;
    device_attr->mesh_shader_enabled=mesh_shader_ext_enabled&&(mesh_shader_supported||optimistic_mesh_only_enable);
    device_attr->task_shader_enabled=mesh_shader_ext_enabled&&(task_shader_supported||optimistic_mesh_only_enable);

    device_attr->pfn_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(device_attr->device, "vkCmdBeginRenderingKHR");
    device_attr->pfn_vkCmdEndRenderingKHR   = (PFN_vkCmdEndRenderingKHR)  vkGetDeviceProcAddr(device_attr->device, "vkCmdEndRenderingKHR");

#ifdef VK_EXT_mesh_shader
    if(device_attr->mesh_shader_extension)
    {
        device_attr->pfn_vkCmdDrawMeshTasksEXT=
            (PFN_vkCmdDrawMeshTasksEXT)vkGetDeviceProcAddr(device_attr->device,"vkCmdDrawMeshTasksEXT");

        device_attr->pfn_vkCmdDrawMeshTasksIndirectEXT=
            (PFN_vkCmdDrawMeshTasksIndirectEXT)vkGetDeviceProcAddr(device_attr->device,"vkCmdDrawMeshTasksIndirectEXT");

        device_attr->pfn_vkCmdDrawMeshTasksIndirectCountEXT=
            (PFN_vkCmdDrawMeshTasksIndirectCountEXT)vkGetDeviceProcAddr(device_attr->device,"vkCmdDrawMeshTasksIndirectCountEXT");
    }
#endif

    GLogInfo("[VulkanDeviceCreater] MeshShader enabled state: extension=%s mesh=%s task=%s",
            BoolText(device_attr->mesh_shader_extension),
            BoolText(device_attr->mesh_shader_enabled),
            BoolText(device_attr->task_shader_enabled));

#ifdef VK_EXT_mesh_shader
    if(device_attr->mesh_shader_extension)
    {
        GLogInfo("[VulkanDeviceCreater] MeshShader command proc: DrawMeshTasks=%s DrawMeshTasksIndirect=%s DrawMeshTasksIndirectCount=%s",
                BoolText(device_attr->pfn_vkCmdDrawMeshTasksEXT!=nullptr),
                BoolText(device_attr->pfn_vkCmdDrawMeshTasksIndirectEXT!=nullptr),
                BoolText(device_attr->pfn_vkCmdDrawMeshTasksIndirectCountEXT!=nullptr));

        if(require.meshShaderOnlyMode
        &&(!device_attr->pfn_vkCmdDrawMeshTasksEXT
        || !device_attr->mesh_shader_enabled
        || !device_attr->task_shader_enabled))
        {
            GLogError("[VulkanDeviceCreater] MeshShaderOnly mode requested but runtime mesh/task path is unavailable (DrawMeshTasks=%s mesh=%s task=%s)",
                     BoolText(device_attr->pfn_vkCmdDrawMeshTasksEXT!=nullptr),
                     BoolText(device_attr->mesh_shader_enabled),
                     BoolText(device_attr->task_shader_enabled));
            return(nullptr);
        }
    }
#endif

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
            device_attr->debug_utils->SetPipelineCache(device_attr->pipeline_cache,"Main GraphicsPipeline Cache");
        }
    #endif//_DEBUG

    VulkanDevice *device = new VulkanDevice(device_attr);

    VmaAllocatorCreateInfo allocator_ci{};
    allocator_ci.physicalDevice  = physical_device->GetVulkanDevice();
    allocator_ci.device          = device_attr->device;
    allocator_ci.instance        = instance->GetVulkanInstance();
    allocator_ci.vulkanApiVersion= VK_API_VERSION_1_3;

    if(vmaCreateAllocator(&allocator_ci, &device->vma_allocator) != VK_SUCCESS)
    {
        delete device;
        return(nullptr);
    }

    return device;
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

    const bool mesh_shader_ext=physical_device->SupportMeshShaderExtension();
    bool mesh_shader_feature=physical_device->SupportMeshShader();
    bool task_shader_feature=physical_device->SupportTaskShader();

#ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    ProbeMeshShaderFeatureBits(physical_device,mesh_shader_feature,task_shader_feature);
#endif

    if(require.meshShaderOnlyMode&&!mesh_shader_ext)
    {
        GLogError("[VulkanDeviceCreater] MeshShaderOnly mode requested, but device '%s' does not support required VK_EXT_mesh_shader extension (extension=%s mesh=%s task=%s)",
                 physical_device->GetDeviceName(),
                 BoolText(mesh_shader_ext),
                 BoolText(mesh_shader_feature),
                 BoolText(task_shader_feature));

        return false;
    }

    if(require.meshShaderOnlyMode&&(!mesh_shader_feature||!task_shader_feature))
    {
        GLogInfo("[VulkanDeviceCreater] MeshShaderOnly mode: probe reports mesh=%s task=%s on '%s'; continuing to vkCreateDevice for definitive support validation.",
                 BoolText(mesh_shader_feature),
                 BoolText(task_shader_feature),
                 physical_device->GetDeviceName());

        mesh_shader_feature=true;
        task_shader_feature=true;
    }

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

    VHRC(meshShader,mesh_shader_ext&&mesh_shader_feature);
    VHRC(taskShader,mesh_shader_ext&&task_shader_feature);

    VHRC_PDE(dynamicState[0],      EXT_EXTENDED_DYNAMIC_STATE);
    VHRC_PDE(dynamicState[1],      EXT_EXTENDED_DYNAMIC_STATE_2);
    VHRC_PDE(dynamicState[2],      EXT_EXTENDED_DYNAMIC_STATE_3);

#undef VHRC_PDE
#undef VHRC_F13
#undef VHRC_F10
#undef VHRC

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

    SetShaderCompilerPhysicalDeviceProfile(physical_device->GetPhysicalDeviceProfile());

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
