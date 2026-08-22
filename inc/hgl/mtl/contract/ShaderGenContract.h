#pragma once

namespace hgl::graph::mtl {}

#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl::contract
{
    using namespace hgl::graph::mtl;
    enum class ResourceClass : uint8_t
    {
        Unknown = 0,
        UniformBuffer,
        StorageBuffer,
        SampledImage,
        Sampler,
        CombinedImageSampler,
        InputAttachment,
    };

    struct ResourceRequirement
    {
        std::string name;
        ResourceClass resource_class = ResourceClass::Unknown;
        bool required = true;
    };

    struct PhysicalDeviceLimitsLite
    {
        uint32_t max_image_dimension_2d = 0;
        uint32_t max_push_constants_size = 0;
        uint32_t max_vertex_input_attributes = 0;
        uint32_t max_bound_descriptor_sets = 0;
        uint64_t max_uniform_buffer_range = 0;
        uint64_t max_storage_buffer_range = 0;

        // VK_EXT_mesh_shader limits（GLSLCompiler 编译 mesh shader 的 resource limits）
        uint32_t max_mesh_output_vertices = 0;
        uint32_t max_mesh_output_primitives = 0;
        uint32_t max_mesh_work_group_size_x = 0;
        uint32_t max_mesh_work_group_size_y = 0;
        uint32_t max_mesh_work_group_size_z = 0;
        uint32_t max_task_work_group_size_x = 0;
        uint32_t max_task_work_group_size_y = 0;
        uint32_t max_task_work_group_size_z = 0;
        uint32_t max_mesh_view_count = 0;
    };

    struct PhysicalDeviceFeaturesLite
    {
        bool geometry_shader = false;
        bool tessellation_shader = false;
        bool wide_lines = false;
        bool sampler_anisotropy = false;
    };

    struct PhysicalDeviceProfileLite
    {
        std::string name;
        std::string device_type;
        std::string capability_tier;

        uint32_t vendor_id = 0;
        uint32_t device_id = 0;
        uint32_t api_version = 0;
        uint32_t queue_family_count = 0;

        PhysicalDeviceLimitsLite limits;
        PhysicalDeviceFeaturesLite features;
    };
}
