#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>

#ifdef _DEBUG
#include<iostream>
#include<iomanip>
#endif//_DEBUG

VK_NAMESPACE_BEGIN
namespace
{
    template<typename T> T Clamp(const T &cur,const T &min_value,const T &max_value)
    {
        if(cur<min_value)return min_value;
        if(cur>max_value)return max_value;

        return cur;
    }

    VkExtent2D GetSwapchainExtent(VkSurfaceCapabilitiesKHR &surface_caps,uint32_t width,uint32_t height)
    {
        if(surface_caps.currentExtent.width==UINT32_MAX)
        {
            VkExtent2D swapchain_extent;

            swapchain_extent.width=Clamp(width,surface_caps.minImageExtent.width,surface_caps.maxImageExtent.width);
            swapchain_extent.height=Clamp(height,surface_caps.minImageExtent.height,surface_caps.maxImageExtent.height);

            return swapchain_extent;
        }
        else
        {
            return surface_caps.currentExtent;
        }
    }

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

        return(nullptr);
    }

    VkSwapchainKHR CreateSwapChain(DeviceAttribute *rsa)
    {
        VkSwapchainCreateInfoKHR swapchain_ci={};

        swapchain_ci.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_ci.pNext=nullptr;
        swapchain_ci.surface=rsa->surface;
        swapchain_ci.minImageCount=3;//rsa->surface_caps.minImageCount;
        swapchain_ci.imageFormat=rsa->format;
        swapchain_ci.imageExtent=rsa->swapchain_extent;
        swapchain_ci.preTransform=rsa->preTransform;
        swapchain_ci.compositeAlpha=rsa->compositeAlpha;
        swapchain_ci.imageArrayLayers=1;
        swapchain_ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;
        swapchain_ci.oldSwapchain=VK_NULL_HANDLE;
        swapchain_ci.clipped=true;
        swapchain_ci.imageColorSpace=VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        swapchain_ci.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[2]={rsa->graphics_family, rsa->present_family};
        if(rsa->graphics_family!=rsa->present_family)
        {
            // If the graphics and present queues are from different queue families,
            // we either have to explicitly transfer ownership of images between
            // the queues, or we have to create the swapchain with imageSharingMode
            // as VK_SHARING_MODE_CONCURRENT
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_CONCURRENT;
            swapchain_ci.queueFamilyIndexCount=2;
            swapchain_ci.pQueueFamilyIndices=queueFamilyIndices;
        }
        else
        {
            swapchain_ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
        }

        VkSwapchainKHR swap_chain;

        if(vkCreateSwapchainKHR(rsa->device,&swapchain_ci,nullptr,&swap_chain)==VK_SUCCESS)
            return(swap_chain);

        return(nullptr);
    }

    ImageView *Create2DImageView(VkDevice device,VkFormat format,VkImage img=nullptr)
    {
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,VK_IMAGE_ASPECT_COLOR_BIT,img);
    }

    ImageView *CreateDepthImageView(VkDevice device,VkFormat format,VkImage img=nullptr)
    {
        return CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,VK_IMAGE_ASPECT_DEPTH_BIT,img);
    }

    bool CreateSwapchainImageView(DeviceAttribute *rsa)
    {
        uint32_t count;

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,nullptr)!=VK_SUCCESS)
            return(false);

        rsa->sc_images.SetCount(count);

        if(vkGetSwapchainImagesKHR(rsa->device,rsa->swap_chain,&count,rsa->sc_images.GetData())!=VK_SUCCESS)
        {
            rsa->sc_images.Clear();
            return(false);
        }

        VkImage *ip=rsa->sc_images.GetData();
        ImageView *vp;
        for(uint32_t i=0; i<count; i++)
        {
            vp=Create2DImageView(rsa->device,rsa->format,*ip);

            if(vp==nullptr)
                return(false);

            rsa->sc_image_views.Add(vp);

            ++ip;
        }

        return(true);
    }

    bool CreateDepthBuffer(DeviceAttribute *rsa)
    {
        VkImageCreateInfo image_info={};

        const VkFormat depth_format=VK_FORMAT_D16_UNORM;

        const VkFormatProperties props=rsa->physical_device->GetFormatProperties(depth_format);

        if(props.linearTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            image_info.tiling=VK_IMAGE_TILING_LINEAR;
        else
        if(props.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            image_info.tiling=VK_IMAGE_TILING_OPTIMAL;
        else
            return(false);

        image_info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext=nullptr;
        image_info.imageType=VK_IMAGE_TYPE_2D;
        image_info.format=depth_format;
        image_info.extent.width=rsa->swapchain_extent.width;
        image_info.extent.height=rsa->swapchain_extent.height;
        image_info.extent.depth=1;
        image_info.mipLevels=1;
        image_info.arrayLayers=1;
        image_info.samples=VK_SAMPLE_COUNT_1_BIT;
        image_info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.queueFamilyIndexCount=0;
        image_info.pQueueFamilyIndices=nullptr;
        image_info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        image_info.flags=0;

        rsa->depth.format=depth_format;

        if(vkCreateImage(rsa->device,&image_info,nullptr,&rsa->depth.image)!=VK_SUCCESS)
            return(false);

        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(rsa->device,rsa->depth.image,&mem_reqs);

        VkMemoryAllocateInfo mem_alloc={};
        mem_alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mem_alloc.pNext=nullptr;
        mem_alloc.allocationSize=0;
        mem_alloc.memoryTypeIndex=0;
        mem_alloc.allocationSize=mem_reqs.size;

        if(!rsa->CheckMemoryType(mem_reqs.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,&mem_alloc.memoryTypeIndex))
            return(false);

        if(vkAllocateMemory(rsa->device,&mem_alloc,nullptr,&rsa->depth.mem)!=VK_SUCCESS)
            return(false);

        if(vkBindImageMemory(rsa->device,rsa->depth.image,rsa->depth.mem,0)!=VK_SUCCESS)
            return(false);

        rsa->depth.view=CreateDepthImageView(rsa->device,depth_format,rsa->depth.image);

        if(rsa->depth.view==nullptr)
            return(false);

        return(true);
    }

    VkDescriptorPool CreateDescriptorPool(VkDevice device,int sets_count)
    {
        VkDescriptorPoolSize pool_size[]=
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024}
        };

        VkDescriptorPoolCreateInfo dp_create_info={};
        dp_create_info.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dp_create_info.pNext=nullptr;
        dp_create_info.maxSets=sets_count;
        dp_create_info.poolSizeCount=sizeof(pool_size)/sizeof(VkDescriptorPoolSize);
        dp_create_info.pPoolSizes=pool_size;

        VkDescriptorPool desc_pool;

        if(vkCreateDescriptorPool(device,&dp_create_info,nullptr,&desc_pool)!=VK_SUCCESS)
            return(nullptr);

        return desc_pool;
    }

    VkPipelineCache CreatePipelineCache(VkDevice device)
    {
        VkPipelineCacheCreateInfo pipelineCache;
        pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        pipelineCache.pNext = nullptr;
        pipelineCache.initialDataSize = 0;
        pipelineCache.pInitialData = nullptr;
        pipelineCache.flags = 0;

        VkPipelineCache cache;

        if(vkCreatePipelineCache(device, &pipelineCache, nullptr, &cache)!=VK_SUCCESS)
            return(nullptr);

        return cache;
    }

    bool CreateSwapchinAndImageView(DeviceAttribute *attr)
    {
        attr->swap_chain=CreateSwapChain(attr);

        if(!attr->swap_chain)
            return(false);

        if(!CreateSwapchainImageView(attr))
            return(false);

        if(!CreateDepthBuffer(attr))
            return(false);

        return(true);
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

Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,uint width,uint height)
{
    #ifdef _DEBUG
    {
        const VkDriverIdKHR driver_id=physical_device->GetDriverId();

        if(driver_id>=VK_DRIVER_ID_BEGIN_RANGE_KHR
         &&driver_id<=VK_DRIVER_ID_END_RANGE_KHR)
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

    DeviceAttribute *attr=new DeviceAttribute(inst,physical_device,surface);

    AutoDelete<DeviceAttribute> auto_delete(attr);

    attr->swapchain_extent=GetSwapchainExtent(attr->surface_caps,width,height);

    if(attr->graphics_family==ERROR_FAMILY_INDEX)
        return(nullptr);

    attr->device=CreateDevice(inst,*physical_device,attr->graphics_family);

    if(!attr->device)
        return(nullptr);

    GetDeviceQueue(attr);

    attr->cmd_pool=CreateCommandPool(attr->device,attr->graphics_family);

    if(!attr->cmd_pool)
        return(nullptr);

    if(!CreateSwapchinAndImageView(attr))
        return(nullptr);

    attr->desc_pool=CreateDescriptorPool(attr->device,1024);

    if(!attr->desc_pool)
        return(nullptr);

    attr->pipeline_cache=CreatePipelineCache(attr->device);

    if(!attr->pipeline_cache)
        return(nullptr);

    auto_delete.Clear();

    return(new Device(attr));
}

bool ResizeRenderDevice(DeviceAttribute *attr,uint width,uint height)
{
    if(attr->swapchain_extent.width==width
     &&attr->swapchain_extent.height==height)
        return(true);

    attr->ClearSwapchain();

    attr->Refresh();

    attr->swapchain_extent=GetSwapchainExtent(attr->surface_caps,width,height);

    return CreateSwapchinAndImageView(attr);
}
VK_NAMESPACE_END
