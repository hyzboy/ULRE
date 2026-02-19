#pragma once

#include<vulkan/vulkan.h>

namespace hgl
{
    constexpr const char *ToString(const VkObjectType &type)
    {
        switch (type)
        {
            case VK_OBJECT_TYPE_UNKNOWN: return "Unknown";
            case VK_OBJECT_TYPE_INSTANCE: return "Instance";
            case VK_OBJECT_TYPE_PHYSICAL_DEVICE: return "PhysicalDevice";
            case VK_OBJECT_TYPE_DEVICE: return "Device";
            case VK_OBJECT_TYPE_QUEUE: return "Queue";
            case VK_OBJECT_TYPE_SEMAPHORE: return "Semaphore";
            case VK_OBJECT_TYPE_COMMAND_BUFFER: return "CommandBuffer";
            case VK_OBJECT_TYPE_FENCE: return "Fence";
            case VK_OBJECT_TYPE_DEVICE_MEMORY: return "DeviceMemory";
            case VK_OBJECT_TYPE_BUFFER: return "Buffer";
            case VK_OBJECT_TYPE_IMAGE: return "Image";
            case VK_OBJECT_TYPE_EVENT: return "Event";
            case VK_OBJECT_TYPE_QUERY_POOL: return "QueryPool";
            case VK_OBJECT_TYPE_BUFFER_VIEW: return "BufferView";
            case VK_OBJECT_TYPE_IMAGE_VIEW: return "ImageView";
            case VK_OBJECT_TYPE_SHADER_MODULE: return "ShaderModule";
            case VK_OBJECT_TYPE_PIPELINE_CACHE: return "PipelineCache";
            case VK_OBJECT_TYPE_PIPELINE_LAYOUT: return "PipelineLayout";
            case VK_OBJECT_TYPE_RENDER_PASS: return "RenderPass";
            case VK_OBJECT_TYPE_PIPELINE: return "Pipeline";
            case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: return "DescriptorSetLayout";
            case VK_OBJECT_TYPE_SAMPLER: return "Sampler";
            case VK_OBJECT_TYPE_DESCRIPTOR_POOL: return "DescriptorPool";
            case VK_OBJECT_TYPE_DESCRIPTOR_SET: return "DescriptorSet";
            case VK_OBJECT_TYPE_FRAMEBUFFER: return "Framebuffer";
            case VK_OBJECT_TYPE_COMMAND_POOL: return "CommandPool";
            case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION: return "SamplerYcbcrConversion";
            case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE: return "DescriptorUpdateTemplate";
            case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT: return "PrivateDataSlot";
            case VK_OBJECT_TYPE_SURFACE_KHR: return "SurfaceKHR";
            case VK_OBJECT_TYPE_SWAPCHAIN_KHR: return "SwapchainKHR";
            case VK_OBJECT_TYPE_DISPLAY_KHR: return "DisplayKHR";
            case VK_OBJECT_TYPE_DISPLAY_MODE_KHR: return "DisplayModeKHR";
            case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT: return "DebugReportCallbackEXT";
            case VK_OBJECT_TYPE_VIDEO_SESSION_KHR: return "VideoSessionKHR";
            case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR: return "VideoSessionParametersKHR";
            case VK_OBJECT_TYPE_CU_MODULE_NVX: return "CuModuleNVX";
            case VK_OBJECT_TYPE_CU_FUNCTION_NVX: return "CuFunctionNVX";
            case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT: return "DebugUtilsMessengerEXT";
            case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR: return "AccelerationStructureKHR";
            case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT: return "ValidationCacheEXT";
            case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV: return "AccelerationStructureNV";
            case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL: return "PerformanceConfigurationINTEL";
            case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR: return "DeferredOperationKHR";
            case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV: return "IndirectCommandsLayoutNV";
            case VK_OBJECT_TYPE_CUDA_MODULE_NV: return "CudaModuleNV";
            case VK_OBJECT_TYPE_CUDA_FUNCTION_NV: return "CudaFunctionNV";
            case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA: return "BufferCollectionFUCHSIA";
            case VK_OBJECT_TYPE_MICROMAP_EXT: return "MicromapEXT";
            case VK_OBJECT_TYPE_OPTICAL_FLOW_SESSION_NV: return "OpticalFlowSessionNV";
            case VK_OBJECT_TYPE_SHADER_EXT: return "ShaderEXT";
            case VK_OBJECT_TYPE_PIPELINE_BINARY_KHR: return "PipelineBinaryKHR";
            case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_EXT: return "IndirectCommandsLayoutEXT";
            case VK_OBJECT_TYPE_INDIRECT_EXECUTION_SET_EXT: return "IndirectExecutionSetEXT";
            default: return "Unknown";
        }
    }
}//namespace hgl