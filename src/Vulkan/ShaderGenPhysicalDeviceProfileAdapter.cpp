#include <hgl/shadergen/contract/ShaderGenPhysicalDeviceProfileAdapter.h>

#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/vk/VKPhysicalDevice.h>

namespace hgl::graph::shadergen::contract
{
    const char *ResolveCapabilityTier(const ::hgl::graph::VulkanPhyDevice &pd)
    {
        const auto &limits = pd.GetLimits();

        const bool high =
            pd.isDiscreteGPU() &&
            limits.maxImageDimension2D >= 8192 &&
            limits.maxUniformBufferRange >= (64u * 1024u) &&
            limits.maxStorageBufferRange >= (64u * 1024u * 1024u);

        if (high)
            return "high";

        const bool medium =
            limits.maxImageDimension2D >= 4096 &&
            limits.maxUniformBufferRange >= (32u * 1024u);

        if (medium)
            return "medium";

        return "low";
    }

    const char *ResolveDeviceTypeName(const uint32_t physical_device_type)
    {
        switch (physical_device_type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
            case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "other";
            default: return "unknown";
        }
    }

    PhysicalDeviceProfileLite BuildPhysicalDeviceProfileFromVulkanPhyDevice(const ::hgl::graph::VulkanPhyDevice &pd)
    {
        PhysicalDeviceProfileLite profile;

        const auto &props = pd.GetProperties();
        const auto &limits = pd.GetLimits();
        const auto &f10 = pd.GetFeatures10();
        const auto &f12 = pd.GetFeatures12();
        const auto &f14 = pd.GetFeatures14();

        profile.name = pd.GetDeviceName() ? pd.GetDeviceName() : "";
        profile.device_type = ResolveDeviceTypeName(pd.GetDeviceType());
        profile.capability_tier = ResolveCapabilityTier(pd);

        profile.vendor_id = props.vendorID;
        profile.device_id = props.deviceID;
        profile.api_version = pd.GetVulkanVersion();

        ResolveShaderTargetVersionsFromApi(profile.api_version,
                                           pd.CheckExtensionSupport(VK_KHR_SPIRV_1_4_EXTENSION_NAME),
                                           profile.target_vulkan_version,
                                           profile.target_spv_version);

        profile.queue_family_count = static_cast<uint32_t>(pd.GetQueueFamilyProperties().GetCount());

        profile.limits.max_image_dimension_2d = limits.maxImageDimension2D;
        profile.limits.max_uniform_buffer_range = limits.maxUniformBufferRange;
        profile.limits.max_storage_buffer_range = limits.maxStorageBufferRange;
        profile.limits.max_push_constants_size = limits.maxPushConstantsSize;
        profile.limits.max_vertex_input_attributes = limits.maxVertexInputAttributes;
        profile.limits.max_bound_descriptor_sets = limits.maxBoundDescriptorSets;

        profile.features.geometry_shader = f10.geometryShader;
        profile.features.tessellation_shader = f10.tessellationShader;
        profile.features.wide_lines = f10.wideLines;
        profile.features.sampler_anisotropy = f10.samplerAnisotropy;
        profile.features.index_type_uint8 = f14.indexTypeUint8;
        profile.features.descriptor_indexing = f12.descriptorIndexing;
        profile.features.sampler_mirror_clamp_to_edge = f12.samplerMirrorClampToEdge;

        return profile;
    }
}
