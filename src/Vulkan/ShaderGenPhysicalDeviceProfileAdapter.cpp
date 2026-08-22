#include <hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h>

#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/vk/VKPhysicalDevice.h>

namespace hgl::graph::mtl::contract
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

        profile.name = pd.GetDeviceName() ? pd.GetDeviceName() : "";
        profile.device_type = ResolveDeviceTypeName(pd.GetDeviceType());
        profile.capability_tier = ResolveCapabilityTier(pd);

        profile.vendor_id = props.vendorID;
        profile.device_id = props.deviceID;
        profile.api_version = pd.GetVulkanVersion();

        profile.queue_family_count = static_cast<uint32_t>(pd.GetQueueFamilyProperties().GetCount());

        profile.limits.max_image_dimension_2d = limits.maxImageDimension2D;
        profile.limits.max_uniform_buffer_range = limits.maxUniformBufferRange;
        profile.limits.max_storage_buffer_range = limits.maxStorageBufferRange;
        profile.limits.max_push_constants_size = limits.maxPushConstantsSize;
        profile.limits.max_vertex_input_attributes = limits.maxVertexInputAttributes;
        profile.limits.max_bound_descriptor_sets = limits.maxBoundDescriptorSets;

        // VK_EXT_mesh_shader limits（GLSLCompiler 编译 mesh shader 的 resource limits）
        {
            const auto &mesh_props = pd.GetMeshShaderProperties();

            profile.limits.max_mesh_output_vertices    = mesh_props.maxMeshOutputVertices;
            profile.limits.max_mesh_output_primitives  = mesh_props.maxMeshOutputPrimitives;
            profile.limits.max_mesh_work_group_size_x  = mesh_props.maxMeshWorkGroupSize[0];
            profile.limits.max_mesh_work_group_size_y  = mesh_props.maxMeshWorkGroupSize[1];
            profile.limits.max_mesh_work_group_size_z  = mesh_props.maxMeshWorkGroupSize[2];
            profile.limits.max_task_work_group_size_x  = mesh_props.maxTaskWorkGroupSize[0];
            profile.limits.max_task_work_group_size_y  = mesh_props.maxTaskWorkGroupSize[1];
            profile.limits.max_task_work_group_size_z  = mesh_props.maxTaskWorkGroupSize[2];
            profile.limits.max_mesh_view_count         = mesh_props.maxMeshMultiviewViewCount;
        }

        profile.features.geometry_shader = f10.geometryShader;
        profile.features.tessellation_shader = f10.tessellationShader;
        profile.features.wide_lines = f10.wideLines;
        profile.features.sampler_anisotropy = f10.samplerAnisotropy;

        return profile;
    }
}
