#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKDevice.h>

#include<iostream>
#include<iomanip>

VK_NAMESPACE_BEGIN
Swapchain *CreateSwapchain(const DeviceAttribute *attr,const VkExtent2D &acquire_extent);

namespace
{
    VkDevice CreateDevice(VkInstance instance,VkPhysicalDevice physical_device,uint32_t graphics_family)
    {
        float queue_priorities[1]={0.0};

        VkDeviceQueueCreateInfo queue_info;
        queue_info.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.pNext=nullptr;
        queue_info.queueFamilyIndex=graphics_family;
        queue_info.queueCount=1;
        queue_info.pQueuePriorities=queue_priorities;
        queue_info.flags=0;     //如果这里写VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT，会导致vkGetDeviceQueue调用崩溃

        VkDeviceCreateInfo create_info={};
        const char *ext_list[1]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        create_info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext=nullptr;
        create_info.queueCreateInfoCount=1;
        create_info.pQueueCreateInfos=&queue_info;
        create_info.enabledExtensionCount=1;
        create_info.ppEnabledExtensionNames=ext_list;
        create_info.enabledLayerCount=0;
        create_info.ppEnabledLayerNames=nullptr;
        create_info.pEnabledFeatures=nullptr;

        VkDevice device;

        if(vkCreateDevice(physical_device,&create_info,nullptr,&device)==VK_SUCCESS)
            return device;

        return nullptr;
    }

    void GetDeviceQueue(DeviceAttribute *attr)
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

        cmd_pool_info.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.pNext=nullptr;
        cmd_pool_info.queueFamilyIndex=graphics_family;
        cmd_pool_info.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;        //允许COMMAND被重复begin，如果没有此标记也可以正常用，但是会频繁报错

        VkCommandPool cmd_pool;

        if(vkCreateCommandPool(device,&cmd_pool_info,nullptr,&cmd_pool)==VK_SUCCESS)
            return cmd_pool;

        return(VK_NULL_HANDLE);
    }

    ImageView *Create2DImageView(VkDevice device,VkFormat format,const VkExtent2D &ext,VkImage img=VK_NULL_HANDLE)
    {
        VkExtent3D extent;

        copy(extent,ext);

        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,extent,VK_IMAGE_ASPECT_COLOR_BIT,img);
    }

    ImageView *CreateDepthImageView(VkDevice device,VkFormat format,const VkExtent2D &ext,VkImage img=VK_NULL_HANDLE)
    {
        VkExtent3D extent;

        copy(extent,ext);
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,extent,VK_IMAGE_ASPECT_DEPTH_BIT,img);
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

    VkPipelineCache CreatePipelineCache(VkDevice device)
    {
        VkPipelineCacheCreateInfo pipelineCache;
        pipelineCache.sType             = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pipelineCache.pNext             = nullptr;
        pipelineCache.flags             = 0;
        pipelineCache.initialDataSize   = 0;
        pipelineCache.pInitialData      = nullptr;

        VkPipelineCache cache;

        if(vkCreatePipelineCache(device, &pipelineCache, nullptr, &cache)!=VK_SUCCESS)
            return(VK_NULL_HANDLE);

        return cache;
    }

    void DebugOut(const VkPhysicalDeviceFeatures &features)
    {
    #define OUTPUT_PHYSICAL_DEVICE_FEATURE(name)    std::cout<<std::setw(40)<<std::right<<#name<<": "<<(features.name?"true":"false")<<std::endl;
            OUTPUT_PHYSICAL_DEVICE_FEATURE(robustBufferAccess)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(fullDrawIndexUint32)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(imageCubeArray)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(independentBlend)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(geometryShader)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(tessellationShader)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sampleRateShading)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(dualSrcBlend)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(logicOp)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(multiDrawIndirect)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(drawIndirectFirstInstance)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(depthClamp)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(depthBiasClamp)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(fillModeNonSolid)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(depthBounds)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(wideLines)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(largePoints)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(alphaToOne)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(multiViewport)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(samplerAnisotropy)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(textureCompressionETC2)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(textureCompressionASTC_LDR)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(textureCompressionBC)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(occlusionQueryPrecise)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(pipelineStatisticsQuery)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(vertexPipelineStoresAndAtomics)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(fragmentStoresAndAtomics)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderTessellationAndGeometryPointSize)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderImageGatherExtended)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageExtendedFormats)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageMultisample)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageReadWithoutFormat)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageWriteWithoutFormat)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderUniformBufferArrayDynamicIndexing)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderSampledImageArrayDynamicIndexing)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageBufferArrayDynamicIndexing)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageArrayDynamicIndexing)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderClipDistance)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderCullDistance)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderFloat64)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderInt64)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderInt16)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderResourceResidency)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderResourceMinLod)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseBinding)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidencyBuffer)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidencyImage2D)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidencyImage3D)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidency2Samples)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidency4Samples)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidency8Samples)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidency16Samples)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(sparseResidencyAliased)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(variableMultisampleRate)
            OUTPUT_PHYSICAL_DEVICE_FEATURE(inheritedQueries)
    #undef OUTPUT_PHYSICAL_DEVICE_FEATURE
    }

    void DebugOutVersion(uint32_t version)
    {
        std::cout<<VK_VERSION_MAJOR(version)<<"."<<VK_VERSION_MINOR(version)<<"."<<VK_VERSION_PATCH(version)<<std::endl;
    }

    void DebugOut(const VkPhysicalDeviceLimits &limits)
    {
    #define OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(name) std::cout<<std::setw(60)<<std::right<<#name<<": "<<limits.name<<std::endl;
    #define OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2(name) std::cout<<std::setw(60)<<std::right<<#name<<": "<<limits.name[0]<<", "<<limits.name[1]<<std::endl;
    #define OUT_PHYSICAL_DEVICE_LIMIT_VECTOR3(name) std::cout<<std::setw(60)<<std::right<<#name<<": "<<limits.name[0]<<", "<<limits.name[1]<<", "<<limits.name[2]<<std::endl;
    #define OUT_PHYSICAL_DEVICE_LIMIT_BOOLEAN(name) std::cout<<std::setw(60)<<std::right<<#name<<": "<<(limits.name?"true":"false")<<std::endl;

        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxImageDimension1D)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxImageDimension2D)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxImageDimension3D)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxImageDimensionCube)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxImageArrayLayers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTexelBufferElements)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxUniformBufferRange)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxStorageBufferRange)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPushConstantsSize)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxMemoryAllocationCount)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxSamplerAllocationCount)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(bufferImageGranularity)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(sparseAddressSpaceSize)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxBoundDescriptorSets)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorSamplers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorUniformBuffers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorStorageBuffers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorSampledImages)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorStorageImages)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageDescriptorInputAttachments)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxPerStageResources)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetSamplers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetUniformBuffers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetUniformBuffersDynamic)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetStorageBuffers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetStorageBuffersDynamic)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetSampledImages)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetStorageImages)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDescriptorSetInputAttachments)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxVertexInputAttributes)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxVertexInputBindings)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxVertexInputAttributeOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxVertexInputBindingStride)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxVertexOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationGenerationLevel)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationPatchSize)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationControlPerVertexInputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationControlPerVertexOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationControlPerPatchOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationControlTotalOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationEvaluationInputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTessellationEvaluationOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxGeometryShaderInvocations)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxGeometryInputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxGeometryOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxGeometryOutputVertices)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxGeometryTotalOutputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFragmentInputComponents)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFragmentOutputAttachments)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFragmentDualSrcAttachments)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFragmentCombinedOutputResources)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxComputeSharedMemorySize)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR3(maxComputeWorkGroupCount)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxComputeWorkGroupInvocations)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR3(maxComputeWorkGroupSize)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(subPixelPrecisionBits)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(subTexelPrecisionBits)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(mipmapPrecisionBits)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDrawIndexedIndexValue)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxDrawIndirectCount)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxSamplerLodBias)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxSamplerAnisotropy)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxViewports)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2(maxViewportDimensions)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2(viewportBoundsRange)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(viewportSubPixelBits)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minMemoryMapAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minTexelBufferOffsetAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minUniformBufferOffsetAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minStorageBufferOffsetAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minTexelOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTexelOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minTexelGatherOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxTexelGatherOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(minInterpolationOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxInterpolationOffset)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(subPixelInterpolationOffsetBits)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFramebufferWidth)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFramebufferHeight)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxFramebufferLayers)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(framebufferColorSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(framebufferDepthSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(framebufferStencilSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(framebufferNoAttachmentsSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxColorAttachments)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(sampledImageColorSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(sampledImageIntegerSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(sampledImageDepthSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(sampledImageStencilSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(storageImageSampleCounts)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxSampleMaskWords)
        OUT_PHYSICAL_DEVICE_LIMIT_BOOLEAN(timestampComputeAndGraphics)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(timestampPeriod)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxClipDistances)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxCullDistances)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(maxCombinedClipAndCullDistances)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(discreteQueuePriorities)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2(pointSizeRange)
        OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2(lineWidthRange)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(pointSizeGranularity)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(lineWidthGranularity)
        OUT_PHYSICAL_DEVICE_LIMIT_BOOLEAN(strictLines)
        OUT_PHYSICAL_DEVICE_LIMIT_BOOLEAN(standardSampleLocations)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(optimalBufferCopyOffsetAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(optimalBufferCopyRowPitchAlignment)
        OUT_PHYSICAL_DEVICE_LIMIT_INTEGER(nonCoherentAtomSize)

#undef OUT_PHYSICAL_DEVICE_LIMIT_BOOLEAN
#undef OUT_PHYSICAL_DEVICE_LIMIT_VECTOR3
#undef OUT_PHYSICAL_DEVICE_LIMIT_VECTOR2
#undef OUT_PHYSICAL_DEVICE_LIMIT_INTEGER
    }

    void DebugOut(const VkPhysicalDeviceProperties &pdp)
    {
        constexpr char DeviceTypeString[VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE][16]=
        {
            "Other",
            "Integrated GPU",
            "Discrete GPU",
            "Virtual GPU",
            "CPU"
        };

        std::cout<<"       apiVersion: ";DebugOutVersion(pdp.apiVersion);
        std::cout<<"    driverVersion: ";DebugOutVersion(pdp.driverVersion);
        std::cout<<"         vendorID: "<<pdp.vendorID<<std::endl;
        std::cout<<"         deviceID: "<<pdp.deviceID<<std::endl;
        std::cout<<"       deviceType: "<<DeviceTypeString[pdp.deviceType]<<std::endl;
        std::cout<<"       deviceName: "<<pdp.deviceName<<std::endl;

        UTF8String uuid=HexToString<char>(pdp.pipelineCacheUUID);

        std::cout<<"pipelineCahceUUID: "<<uuid.c_str()<<std::endl;

        DebugOut(pdp.limits);
    }
}//namespace

Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,const VkExtent2D &extent)
{
    #ifdef _DEBUG
    {
        const VkDriverIdKHR driver_id=physical_device->GetDriverId();

        if(driver_id>=VK_DRIVER_ID_BEGIN_RANGE
         &&driver_id<=VK_DRIVER_ID_END_RANGE)
        {
            std::cout<<"DriverID: "<<physical_device->GetDriverId()<<std::endl;
            std::cout<<"DriverName: "<<physical_device->GetDriverName()<<std::endl;
            std::cout<<"DriverInfo: "<<physical_device->GetDriverInfo()<<std::endl;
        }
        else
        {
            std::cout<<"Unknow VideoCard Driver"<<std::endl;
        }

        DebugOut(physical_device->GetProperties());
        DebugOut(physical_device->GetFeatures());
    }
    #endif//_DEBUG

    DeviceAttribute *device_attr=new DeviceAttribute(inst,physical_device,surface);

    AutoDelete<DeviceAttribute> auto_delete(device_attr);

    if(device_attr->graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    device_attr->device=CreateDevice(inst,*physical_device,device_attr->graphics_family);

    if(!device_attr->device)
        return(nullptr);

    GetDeviceQueue(device_attr);

    device_attr->cmd_pool=CreateCommandPool(device_attr->device,device_attr->graphics_family);

    if(!device_attr->cmd_pool)
        return(nullptr);
        
    device_attr->desc_pool=CreateDescriptorPool(device_attr->device,1024);

    if(!device_attr->desc_pool)
        return(nullptr);

    device_attr->pipeline_cache=CreatePipelineCache(device_attr->device);

    if(!device_attr->pipeline_cache)
        return(nullptr);

    auto_delete.Clear();

    return(new Device(device_attr));
}

Device *CreateRenderDevice(vulkan::Instance *inst,Window *win,const vulkan::PhysicalDevice *pd)
{
    if(!inst)
        return(nullptr);

    if(!pd)pd=inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
    if(!pd)pd=inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
    if(!pd)pd=inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡

    if(!pd)
        return(nullptr);

    VkSurfaceKHR surface=CreateVulkanSurface(*inst,win);

    if(!surface)
        return(nullptr);
        
    VkExtent2D extent;
    
    extent.width=win->GetWidth();
    extent.height=win->GetHeight();

    Device *device=CreateRenderDevice(*inst,pd,surface,extent);

    if(!device)
    {
        vkDestroySurfaceKHR(*inst,surface,nullptr);
        return(nullptr);
    }

    return device;
}
VK_NAMESPACE_END
