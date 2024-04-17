#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/VKDevice.h>

#include<iostream>
#include<iomanip>

VK_NAMESPACE_BEGIN
VkPipelineCache CreatePipelineCache(VkDevice device,const VkPhysicalDeviceProperties &);

void SetShaderCompilerVersion(const GPUPhysicalDevice *);

#ifdef _DEBUG
DebugUtils *CreateDebugUtils(VkDevice);

void LogSurfaceFormat(const VkSurfaceFormatKHR *surface_format_list,const uint32_t format_count,const uint32_t select)
{
    const VkSurfaceFormatKHR *sf=surface_format_list;

    std::cout<<"Current physics device support "<<format_count<<" surface format"<<std::endl;

    const VulkanFormat *vf;
    const VulkanColorSpace *cs;

    for(uint32_t i=0;i<format_count;i++)
    {
        vf=GetVulkanFormat(sf->format);
        cs=GetVulkanColorSpace(sf->colorSpace);

        if(select==i)
            std::cout<<"  *";
        else
            std::cout<<"   ";

        std::cout<<std::setw(3)<<i<<": "<<std::setw(10)<<vf->name<<", "<<cs->name<<std::endl;

        ++sf;
    }
}
#endif//_DEBUG

namespace
{
    void SetDeviceExtension(CharPointerList *ext_list,const GPUPhysicalDevice *physical_device,const VulkanHardwareRequirement &require)
    {
        ext_list->Add(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        constexpr char *require_ext_list[]=
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

        if(require.line_rasterization>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

        if(require.texture_compression.pvrtc>=VulkanHardwareRequirement::SupportLevel::Want)                   //前面检测过了，所以这里不用再次检测是否支持
            ext_list->Add(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);

        if(require.uint8_draw_index>=VulkanHardwareRequirement::SupportLevel::Want)
            ext_list->Add(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME);
    }

    void SetDeviceFeatures(VkPhysicalDeviceFeatures *features,const VkPhysicalDeviceFeatures &pdf,const VulkanHardwareRequirement &require)
    {
        #define FEATURE_COPY(name)  features->name=pdf.name;
        #define REQURE_FEATURE_COPY(rn,fn) if(require.rn>=VulkanHardwareRequirement::SupportLevel::Want)features->fn=pdf.fn;

        FEATURE_COPY(multiDrawIndirect);
        FEATURE_COPY(samplerAnisotropy);

        REQURE_FEATURE_COPY(geometry_shader,                geometryShader);

        REQURE_FEATURE_COPY(texture_cube_array,             imageCubeArray);

        REQURE_FEATURE_COPY(uint32_draw_index,              fullDrawIndexUint32);
        REQURE_FEATURE_COPY(sample_rate_shading,            sampleRateShading);

        REQURE_FEATURE_COPY(fill_mode_non_solid,            fillModeNonSolid);

        REQURE_FEATURE_COPY(wide_lines,                     wideLines)
        REQURE_FEATURE_COPY(large_points,                   largePoints)

        REQURE_FEATURE_COPY(texture_compression.bc,         textureCompressionBC);
        REQURE_FEATURE_COPY(texture_compression.etc2,       textureCompressionETC2);
        REQURE_FEATURE_COPY(texture_compression.astc_ldr,   textureCompressionASTC_LDR);

        #undef REQURE_FEATURE_COPY
        #undef FEATURE_COPY
    }

    void GetDeviceQueue(GPUDeviceAttribute *attr)
    {
        vkGetDeviceQueue(attr->device,attr->graphics_family,0,&attr->graphics_queue);

        if(attr->graphics_family==attr->present_family)
            attr->present_queue=attr->graphics_queue;
        else
            vkGetDeviceQueue(attr->device,attr->present_family,0,&attr->present_queue);
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
            return cmd_pool;

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
void OutputPhysicalDeviceCaps(const GPUPhysicalDevice *);
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

    VkDevice device;

    if(vkCreateDevice(*physical_device,&create_info,nullptr,&device)==VK_SUCCESS)
        return device;

    return nullptr;
}

void VulkanDeviceCreater::ChooseSurfaceFormat()
{
    uint32_t format_count;

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, surface, &format_count, nullptr) != VK_SUCCESS)
        return;

    AutoDeleteArray<VkSurfaceFormatKHR> surface_formats_list(format_count);

    if (vkGetPhysicalDeviceSurfaceFormatsKHR(*physical_device, surface, &format_count, surface_formats_list) == VK_SUCCESS)
    {
        int fmt_index=-1;
        int cs_index=-1;
        int fmt;
        int cs;
        uint32_t sel=0;

        for(uint32_t i=0;i<format_count;i++)
        {
            fmt=perfer_color_formats->Find(surface_formats_list[i].format);
            cs=perfer_color_spaces->Find(surface_formats_list[i].colorSpace);

            if((fmt==fmt_index&&cs>cs_index)||fmt>fmt_index)
            {
                surface_format=surface_formats_list[i];

                fmt_index=fmt;
                cs_index=cs;
                sel=i;
            }
        }

        #ifdef _DEBUG
        LogSurfaceFormat(surface_formats_list,format_count,sel);
        #endif//_DEBUG

        if(fmt_index!=-1)
            return;
    }

    surface_format.format=PF_RGBA8s;
    surface_format.colorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

GPUDevice *VulkanDeviceCreater::CreateRenderDevice()
{
    GPUDeviceAttribute *device_attr=new GPUDeviceAttribute(instance,physical_device,surface);

    AutoDelete<GPUDeviceAttribute> auto_delete(device_attr);

    if(device_attr->graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    SetDeviceExtension(&ext_list,physical_device,require);
    SetDeviceFeatures(&features,physical_device->GetFeatures10(),require);

    device_attr->device=CreateDevice(device_attr->graphics_family);

    if(!device_attr->device)
        return(nullptr);

    ChooseSurfaceFormat();

    device_attr->surface_format=surface_format;

    GetDeviceQueue(device_attr);

    device_attr->cmd_pool=CreateCommandPool(device_attr->device,device_attr->graphics_family);

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
            device_attr->debug_utils->SetPhysicalDevice(*physical_device,"Physical Device:"+AnsiString(physical_device->GetDeviceName()));
            device_attr->debug_utils->SetDevice(device_attr->device,"Device:"+AnsiString(physical_device->GetDeviceName()));
            device_attr->debug_utils->SetSurfaceKHR(surface,"Surface");
            device_attr->debug_utils->SetCommandPool(device_attr->cmd_pool,"Main Command Pool");
            device_attr->debug_utils->SetDescriptorPool(device_attr->desc_pool,"Main Descriptor Pool");
            device_attr->debug_utils->SetPipelineCache(device_attr->pipeline_cache,"Main Pipeline Cache");
        }
    #endif//_DEBUG

    return(new GPUDevice(device_attr));
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
        hgl_cpy(require,*req);
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

#define VHR_MINCHECK(name,lname) if(require.name>0&&require.name>limits.lname)return(false);

    VHR_MINCHECK(min_1d_image_size           ,maxImageDimension1D     )
    VHR_MINCHECK(min_2d_image_size           ,maxImageDimension2D     )
    VHR_MINCHECK(min_3d_image_size           ,maxImageDimension3D     )
    VHR_MINCHECK(min_cube_image_size         ,maxImageDimensionCube   )
    VHR_MINCHECK(min_array_image_layers      ,maxImageArrayLayers     )

    VHR_MINCHECK(min_vertex_input_attribute  ,maxVertexInputAttributes)
    VHR_MINCHECK(min_color_attachments       ,maxColorAttachments     )

    VHR_MINCHECK(min_push_constant_size      ,maxPushConstantsSize    )
    VHR_MINCHECK(min_ubo_range               ,maxUniformBufferRange   )
    VHR_MINCHECK(min_ssbo_range              ,maxStorageBufferRange   )

    VHR_MINCHECK(min_draw_indirect_count     ,maxDrawIndirectCount    )

#undef VHR_MINCHECK

    const VkPhysicalDeviceFeatures &features10=physical_device->GetFeatures10();
    const VkPhysicalDeviceVulkan13Features &features13=physical_device->GetFeatures13();

#define VHRC(name,check) if(require.name>=VulkanHardwareRequirement::SupportLevel::Must&&(!check))return(false);

    #define VHRC_F10(name,f10name) VHRC(name,features10.f10name)
    #define VHRC_F13(name,f13name) VHRC(name,features13.f13name)
    #define VHRC_PDE(name,pdename) VHRC(name,physical_device->CheckExtensionSupport(VK_##pdename##_EXTENSION_NAME))

    VHRC_F10(geometry_shader,       geometryShader);
    VHRC_F10(tessellation_shader,   tessellationShader);

    VHRC_F10(multi_draw_indirect,   multiDrawIndirect);

    VHRC_F10(sample_rate_shading,   sampleRateShading);

    VHRC_F10(fill_mode_non_solid,   fillModeNonSolid);

    VHRC_F10(wide_lines,            wideLines);
    VHRC_PDE(line_rasterization,    EXT_LINE_RASTERIZATION);
    VHRC_F10(large_points,          largePoints);

    VHRC_F10(texture_cube_array,    imageCubeArray);

    VHRC_PDE(uint8_draw_index,      EXT_INDEX_TYPE_UINT8);
    VHRC_F10(uint32_draw_index,     fullDrawIndexUint32);

    VHRC_F10(texture_compression.bc,        textureCompressionBC);
    VHRC_F10(texture_compression.etc2,      textureCompressionETC2);
    VHRC_F10(texture_compression.astc_ldr,  textureCompressionASTC_LDR);
    VHRC_F13(texture_compression.astc_hdr,  textureCompressionASTC_HDR);
    VHRC_PDE(texture_compression.pvrtc,     IMG_FORMAT_PVRTC);

    VHRC_F13(dynamic_rendering,     dynamicRendering);

    VHRC_PDE(dynamic_state[0],      EXT_EXTENDED_DYNAMIC_STATE);
    VHRC_PDE(dynamic_state[1],      EXT_EXTENDED_DYNAMIC_STATE_2);
    VHRC_PDE(dynamic_state[2],      EXT_EXTENDED_DYNAMIC_STATE_3);

#undef VHRC_PDE
#undef VHRC_F13
#undef VHRC_F10
#undef VHRC

    return(true);
}

GPUDevice *VulkanDeviceCreater::Create()
{
    if(!instance||!window)
        return(nullptr);

    if(!ChoosePhysicalDevice())
        return(nullptr);

    #ifdef _DEBUG
        OutputPhysicalDeviceCaps(physical_device);
    #endif//_DEBUG

    SetShaderCompilerVersion(physical_device);

    if(!RequirementCheck())
        return(nullptr);

    surface=CreateVulkanSurface(*instance,window);

    if(!surface)
        return(nullptr);

    extent.width    =window->GetWidth();
    extent.height   =window->GetHeight();

    GPUDevice *device=CreateRenderDevice();

    if(!device)
    {
        vkDestroySurfaceKHR(*instance,surface,nullptr);
        return(nullptr);
    }

    return device;
}
VK_NAMESPACE_END
