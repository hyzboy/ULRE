#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl::contract
{
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
    };

    struct PhysicalDeviceFeaturesLite
    {
        bool geometry_shader = false;
        bool tessellation_shader = false;
        bool wide_lines = false;
        bool sampler_anisotropy = false;
        bool index_type_uint8 = false;
        bool descriptor_indexing = false;
        bool sampler_mirror_clamp_to_edge = false;
    };

    struct PhysicalDeviceProfileLite
    {
        std::string name;
        std::string device_type;
        std::string capability_tier;

        uint32_t vendor_id = 0;
        uint32_t device_id = 0;
        uint32_t api_version = 0;
        uint32_t target_vulkan_version = 0;
        uint32_t target_spv_version = 0;
        uint32_t queue_family_count = 0;

        PhysicalDeviceLimitsLite limits;
        PhysicalDeviceFeaturesLite features;
    };
}
