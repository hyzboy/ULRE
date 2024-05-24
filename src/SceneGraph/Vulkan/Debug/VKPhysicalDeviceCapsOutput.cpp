#include<hgl/graph/VK.h>
#include<hgl/graph/VKUUID.h>
#include<hgl/graph/VKPhysicalDevice.h>

#include<iostream>
#include<iomanip>

VK_NAMESPACE_BEGIN
namespace
{
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

    namespace
    {
        struct VulkanDeviceVendor
        {
            VkVendorId id;
            const char *name;
        };

        constexpr const VulkanDeviceVendor vulkan_vendor[]=
        {
            {VK_VENDOR_ID_VIV,"VIV"},
            {VK_VENDOR_ID_VSI,"VSI"},
            {VK_VENDOR_ID_KAZAN,"KAZAN"},
            {VK_VENDOR_ID_CODEPLAY,"CODEPLAY"},
            {VK_VENDOR_ID_MESA,"MESA"},
            {VK_VENDOR_ID_POCL,"POCL"},
            {VK_VENDOR_ID_MOBILEYE,"Mobileye"}
        };

        const char *GetVendorName(const uint32_t id)
        {
            for(const VulkanDeviceVendor &vdv:vulkan_vendor)
            {
                if(vdv.id==id)
                    return vdv.name;
            }

            return "Unknown";
        }
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
        std::cout<<"           vendor: "<<GetVendorName(pdp.vendorID)<<std::endl;
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

void OutputPhysicalDeviceCaps(const GPUPhysicalDevice *pd)
{
    DebugOut(pd->GetProperties());
    DebugOut(pd->GetFeatures10());
    DebugOut(pd->GetFeatures11());
    DebugOut(pd->GetFeatures12());
    DebugOut(pd->GetFeatures13());
}
VK_NAMESPACE_END
