#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDebugMaker.h>


VK_NAMESPACE_BEGIN
VkPipelineCache CreatePipelineCache(VkDevice device,const VkPhysicalDeviceProperties &);

#ifdef _DEBUG
DebugMaker *CreateDebugMaker(VkDevice);
DebugUtils *CreateDebugUtils(VkDevice);
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
//            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,

//            VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
        };

        for(const char *ext_name:require_ext_list)
            if(physical_device->CheckExtensionSupport(ext_name))
                ext_list->Add(ext_name);

        if(require.line_rasterization)
            ext_list->Add(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME);

        if(require.texture_compression.pvrtc)                   //前面检测过了，所以这里不用再次检测是否支持
            ext_list->Add(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME);
    }

    void SetDeviceFeatures(VkPhysicalDeviceFeatures *features,const VkPhysicalDeviceFeatures &pdf,const VulkanHardwareRequirement &require)
    {
        #define FEATURE_COPY(name)  features->name=pdf.name;

        if(require.geometry_shader)     FEATURE_COPY(geometryShader);
//        if(require.compute_shader)      FEATURE_COPY(computeShader);

                                        FEATURE_COPY(multiDrawIndirect);

                                        FEATURE_COPY(samplerAnisotropy);

        if(require.texture_cube_array)  FEATURE_COPY(imageCubeArray);
        if(require.uint32_draw_index)   FEATURE_COPY(fullDrawIndexUint32);
        if(require.wide_lines)          FEATURE_COPY(wideLines)
        if(require.large_points)        FEATURE_COPY(largePoints)

        #undef FEATURE_COPY
    }

    VkDevice CreateDevice(VulkanDeviceCreateInfo *vdci,uint32_t graphics_family)
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
        CharPointerList ext_list;
        VkPhysicalDeviceFeatures features={};

        SetDeviceExtension(&ext_list,vdci->physical_device,vdci->require);
        SetDeviceFeatures(&features,vdci->physical_device->GetFeatures10(),vdci->require);

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

        if(vkCreateDevice(*(vdci->physical_device),&create_info,nullptr,&device)==VK_SUCCESS)
            return device;

        return nullptr;
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
void OutputPhysicalDeviceCaps(GPUPhysicalDevice *);
#endif//_DEBUG

GPUDevice *CreateRenderDevice(VulkanDeviceCreateInfo *vdci,VkSurfaceKHR surface,const VkExtent2D &extent)
{
    #ifdef _DEBUG
        OutputPhysicalDeviceCaps(physical_device);
    #endif//_DEBUG

    GPUDeviceAttribute *device_attr=new GPUDeviceAttribute(vdci->instance,vdci->physical_device,surface);

    AutoDelete<GPUDeviceAttribute> auto_delete(device_attr);

    if(device_attr->graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    device_attr->device=CreateDevice(vdci,device_attr->graphics_family);

    if(!device_attr->device)
        return(nullptr);

#ifdef _DEBUG
    device_attr->debug_maker=CreateDebugMaker(device_attr->device);
    device_attr->debug_utils=CreateDebugUtils(device_attr->device);
#endif//_DEBUG

    GetDeviceQueue(device_attr);

    device_attr->cmd_pool=CreateCommandPool(device_attr->device,device_attr->graphics_family);

    if(!device_attr->cmd_pool)
        return(nullptr);
        
    device_attr->desc_pool=CreateDescriptorPool(device_attr->device,1024);

    if(!device_attr->desc_pool)
        return(nullptr);

    device_attr->pipeline_cache=CreatePipelineCache(device_attr->device,vdci->physical_device->GetProperties());

    if(!device_attr->pipeline_cache)
        return(nullptr);

    auto_delete.Discard();  //discard autodelete

    return(new GPUDevice(device_attr));
}

bool RequirementCheck(const VulkanHardwareRequirement &require,const GPUPhysicalDevice *pd)
{
    const VkPhysicalDeviceLimits &limits=pd->GetLimits();

    if(require.min_1d_image_size        >0&&require.min_1d_image_size       >limits.maxImageDimension1D     )return(false);
    if(require.min_2d_image_size        >0&&require.min_2d_image_size       >limits.maxImageDimension2D     )return(false);
    if(require.min_3d_image_size        >0&&require.min_3d_image_size       >limits.maxImageDimension3D     )return(false);
    if(require.min_cube_image_size      >0&&require.min_cube_image_size     >limits.maxImageDimensionCube   )return(false);
    if(require.min_array_image_layers   >0&&require.min_array_image_layers  >limits.maxImageArrayLayers     )return(false);

    if(require.min_vertex_input_attribute   >0&&require.min_vertex_input_attribute  >limits.maxVertexInputAttributes)return(false);
    if(require.min_color_attachments        >0&&require.min_color_attachments       >limits.maxColorAttachments     )return(false);

    if(require.min_push_constant_size       >0&&require.min_push_constant_size      >limits.maxPushConstantsSize    )return(false);
    if(require.min_ubo_range                >0&&require.min_ubo_range               >limits.maxUniformBufferRange   )return(false);
    if(require.min_ssbo_range               >0&&require.min_ssbo_range              >limits.maxStorageBufferRange   )return(false);

    if(require.min_draw_indirect_count      >0&&require.min_draw_indirect_count     >limits.maxDrawIndirectCount    )return(false);

    const VkPhysicalDeviceFeatures &features10=pd->GetFeatures10();

    if(require.geometry_shader      &&(!features10.geometryShader       ))return(false);
    if(require.tessellation_shader  &&(!features10.tessellationShader   ))return(false);

    if(require.multi_draw_indirect  &&(!features10.multiDrawIndirect    ))return(false);

    if(require.wide_lines           &&(!features10.wideLines            ))return(false);
    if(require.large_points         &&(!features10.largePoints          ))return(false);
    if(require.texture_cube_array   &&(!features10.imageCubeArray       ))return(false);
    if(require.uint32_draw_index    &&(!features10.fullDrawIndexUint32  ))return(false);

    if(require.texture_compression.bc       &&(!features10.textureCompressionBC))return(false);
    if(require.texture_compression.etc2     &&(!features10.textureCompressionETC2))return(false);
    if(require.texture_compression.astc_ldr &&(!features10.textureCompressionASTC_LDR))return(false);

    const VkPhysicalDeviceVulkan13Features &features13=pd->GetFeatures13();

    if(require.dynamic_rendering&&(!features13.dynamicRendering))return(false);

    if(require.texture_compression.astc_hdr &&(!features13.textureCompressionASTC_HDR))return(false);

    if(require.texture_compression.pvrtc&&(!pd->CheckExtensionSupport(VK_IMG_FORMAT_PVRTC_EXTENSION_NAME)))return(false);

    if(require.dynamic_state[0]&&(!pd->CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME   )))return(false);
    if(require.dynamic_state[1]&&(!pd->CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME )))return(false);
    if(require.dynamic_state[2]&&(!pd->CheckExtensionSupport(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME )))return(false);

    if(require.line_rasterization&&(!pd->CheckExtensionSupport(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME)))return(false);

    return(true);
}

GPUDevice *CreateRenderDevice(VulkanDeviceCreateInfo *vdci)
{
    if(!vdci||!vdci->instance)
        return(nullptr);

    const GPUPhysicalDevice *pd=vdci->physical_device;

    if(!pd)pd=vdci->instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
    if(!pd)pd=vdci->instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
    if(!pd)pd=vdci->instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡

    if(!pd)
        return(nullptr);

    vdci->physical_device=pd;

    if(!RequirementCheck(vdci->require,pd))
        return(nullptr);

    VkSurfaceKHR surface=CreateVulkanSurface(*(vdci->instance),vdci->window);

    if(!surface)
        return(nullptr);
        
    VkExtent2D extent;
    
    extent.width    =vdci->window->GetWidth();
    extent.height   =vdci->window->GetHeight();

    GPUDevice *device=CreateRenderDevice(vdci,surface,extent);

    if(!device)
    {
        vkDestroySurfaceKHR(*(vdci->instance), surface, nullptr);
        return(nullptr);
    }

    return device;
}
VK_NAMESPACE_END
