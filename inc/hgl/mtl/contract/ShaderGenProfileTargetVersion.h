#pragma once

namespace hgl::graph::mtl {}

#include <hgl/type/DataType.h>
#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl::contract
{
    using namespace hgl::graph::mtl;

    // 硬性 Vulkan 1.4（vulkan1.4.md 第 1 项）：版本/SPIR-V 阶梯整体塌缩为常量。
    // 1.3 起 SPIR-V 1.6 即核心，1.4 下无条件成立——不再按 api_version 探测。
    constexpr uint32_t MakeVkVersion(const uint32_t major, const uint32_t minor, const uint32_t patch = 0)
    {
        return (major << 22) | (minor << 12) | patch;
    }

    constexpr uint32_t VkVersionMajor(const uint32_t version)
    {
        return version >> 22;
    }

    constexpr uint32_t VkVersionMinor(const uint32_t version)
    {
        return (version >> 12) & 0x3ffu;
    }

    constexpr uint32_t SPV_VERSION_1_6 = (1u << 16) | (6u << 8);
    constexpr uint32_t TARGET_VULKAN_VERSION = MakeVkVersion(1, 4);
    constexpr uint32_t TARGET_SPV_VERSION = SPV_VERSION_1_6;

    // 设备能力哈希（vendor/device/limits/features）——编译目标哈希的超集包含它；
    // 所有 key 维度统一用 GetShaderCompilerProfileHash（目标版本为常量，见上）
    inline uint64 GetPhysicalDeviceProfileHash(
        const PhysicalDeviceProfileLite *profile) noexcept
    {
        hgl::hash::FNV1aHasher64 h;

        if (!profile)
        {
            h << uint32(0);
            return h;
        }

        h << profile->vendor_id
          << profile->device_id
          << profile->api_version
          << profile->limits.max_image_dimension_2d
          << profile->limits.max_push_constants_size
          << profile->limits.max_vertex_input_attributes
          << profile->limits.max_bound_descriptor_sets
          << profile->limits.max_uniform_buffer_range
          << profile->limits.max_storage_buffer_range
          << profile->features.geometry_shader
          << profile->features.tessellation_shader
          << profile->features.wide_lines
          << profile->features.sampler_anisotropy;

        return h;
    }

    inline uint64 GetShaderCompilerProfileHash(
        const PhysicalDeviceProfileLite *profile) noexcept
    {
        uint32 vulkan_version = TARGET_VULKAN_VERSION;
        uint32 spv_version = TARGET_SPV_VERSION;

        hgl::hash::FNV1aHasher64 h;

        h << GetPhysicalDeviceProfileHash(profile)
          << vulkan_version
          << spv_version;

        return h;
    }
}
