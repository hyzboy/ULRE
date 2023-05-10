#include<hgl/platform/Vulkan.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDebugMaker.h>
#include<hgl/graph/VKUUID.h>

#include<iostream>
#include<iomanip>

VK_NAMESPACE_BEGIN
VkPipelineCache CreatePipelineCache(VkDevice device,const VkPhysicalDeviceProperties &);
Swapchain *CreateSwapchain(const GPUDeviceAttribute *attr,const VkExtent2D &acquire_extent);

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

    void DebugOut(const VkPhysicalDeviceFeatures &features)
    {
        std::cout<<"Vulkan 1.0 features"<<std::endl;

    #define OUTPUT_PHYSICAL_DEVICE_FEATURE(name)    std::cout<<std::setw(60)<<std::right<<#name<<": "<<(features.name?"true":"false")<<std::endl;
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

    void DebugOut(const VkPhysicalDeviceVulkan11Features &features)
    {
        std::cout<<"Vulkan 1.1 features"<<std::endl;

#define OUTPUT_PHYSICAL_DEVICE_FEATURE(name)    std::cout<<std::setw(60)<<std::right<<#name<<": "<<(features.name?"true":"false")<<std::endl;
        OUTPUT_PHYSICAL_DEVICE_FEATURE(storageBuffer16BitAccess)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(uniformAndStorageBuffer16BitAccess)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(storagePushConstant16)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(storageInputOutput16)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(multiview)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(multiviewGeometryShader)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(multiviewTessellationShader)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(variablePointersStorageBuffer)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(variablePointers)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(protectedMemory)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(samplerYcbcrConversion)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderDrawParameters)
#undef OUTPUT_PHYSICAL_DEVICE_FEATURE
    }

    void DebugOut(const VkPhysicalDeviceVulkan12Features &features)
    {
        std::cout<<"Vulkan 1.2 features"<<std::endl;

#define OUTPUT_PHYSICAL_DEVICE_FEATURE(name)    std::cout<<std::setw(60)<<std::right<<#name<<": "<<(features.name?"true":"false")<<std::endl;
        OUTPUT_PHYSICAL_DEVICE_FEATURE(samplerMirrorClampToEdge)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(drawIndirectCount)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(storageBuffer8BitAccess)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(uniformAndStorageBuffer8BitAccess)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(storagePushConstant8)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderBufferInt64Atomics)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderSharedInt64Atomics)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderFloat16)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderInt8)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderInputAttachmentArrayDynamicIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderUniformTexelBufferArrayDynamicIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageTexelBufferArrayDynamicIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderUniformBufferArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderSampledImageArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageBufferArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageImageArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderInputAttachmentArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderUniformTexelBufferArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderStorageTexelBufferArrayNonUniformIndexing)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingUniformBufferUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingSampledImageUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingStorageImageUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingStorageBufferUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingUniformTexelBufferUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingStorageTexelBufferUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingUpdateUnusedWhilePending)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingPartiallyBound)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingVariableDescriptorCount)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(runtimeDescriptorArray)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(samplerFilterMinmax)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(scalarBlockLayout)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(imagelessFramebuffer)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(uniformBufferStandardLayout)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderSubgroupExtendedTypes)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(separateDepthStencilLayouts)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(hostQueryReset)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(timelineSemaphore)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(bufferDeviceAddress)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(bufferDeviceAddressCaptureReplay)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(bufferDeviceAddressMultiDevice)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(vulkanMemoryModel)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(vulkanMemoryModelDeviceScope)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(vulkanMemoryModelAvailabilityVisibilityChains)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderOutputViewportIndex)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderOutputLayer)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(subgroupBroadcastDynamicId)
#undef OUTPUT_PHYSICAL_DEVICE_FEATURE
    }
    
    void DebugOut(const VkPhysicalDeviceVulkan13Features &features)
    {
        std::cout<<"Vulkan 1.3 features"<<std::endl;

#define OUTPUT_PHYSICAL_DEVICE_FEATURE(name)    std::cout<<std::setw(60)<<std::right<<#name<<": "<<(features.name?"true":"false")<<std::endl;
        OUTPUT_PHYSICAL_DEVICE_FEATURE(robustImageAccess)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(inlineUniformBlock)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(descriptorBindingInlineUniformBlockUpdateAfterBind)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(pipelineCreationCacheControl)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(privateData)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderDemoteToHelperInvocation)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderTerminateInvocation)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(subgroupSizeControl)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(computeFullSubgroups)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(synchronization2)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(textureCompressionASTC_HDR)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderZeroInitializeWorkgroupMemory)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(dynamicRendering)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(shaderIntegerDotProduct)
        OUTPUT_PHYSICAL_DEVICE_FEATURE(maintenance4)
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

    #ifndef VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE
    #define VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE VK_PHYSICAL_DEVICE_TYPE_OTHER
    #endif//VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE

    #ifndef VK_PHYSICAL_DEVICE_TYPE_END_RANGE
    #define VK_PHYSICAL_DEVICE_TYPE_END_RANGE   VK_PHYSICAL_DEVICE_TYPE_CPU
    #endif//VK_PHYSICAL_DEVICE_TYPE_END_RANGE

    #ifndef VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE
    constexpr size_t VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE=VK_PHYSICAL_DEVICE_TYPE_END_RANGE-VK_PHYSICAL_DEVICE_TYPE_BEGIN_RANGE+1;
    #endif//VK_PHYSICAL_DEVICE_TYPE_RANGE_SIZE

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
        std::cout<<"         vendorID: 0x"<<HexToString<char>(pdp.vendorID).c_str()<<std::endl;
        std::cout<<"         deviceID: 0x"<<HexToString<char>(pdp.deviceID).c_str()<<std::endl;
        std::cout<<"       deviceType: "<<DeviceTypeString[pdp.deviceType]<<std::endl;
        std::cout<<"       deviceName: "<<pdp.deviceName<<std::endl;

        if(memcmp(pdp.pipelineCacheUUID,"rdoc",4)==0)
        {
            std::cout<<"pipelineCahceUUID: "<<(char *)pdp.pipelineCacheUUID<<std::endl;
        }
        else
        {
            AnsiString uuid=VkUUID2String<char>(pdp.pipelineCacheUUID);

            std::cout<<"pipelineCahceUUID: "<<uuid.c_str()<<std::endl;
        }

        DebugOut(pdp.limits);
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

GPUDevice *CreateRenderDevice(VulkanDeviceCreateInfo *vdci,VkSurfaceKHR surface,const VkExtent2D &extent)
{
    #ifdef _DEBUG
    {
        DebugOut(vdci->physical_device->GetProperties());
        DebugOut(vdci->physical_device->GetFeatures10());
        DebugOut(vdci->physical_device->GetFeatures11());
        DebugOut(vdci->physical_device->GetFeatures12());
        DebugOut(vdci->physical_device->GetFeatures13());
    }
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
