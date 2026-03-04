#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph::mtl::contract
{
    constexpr uint32_t SPV_VERSION_1_0 = (1u << 16);
    constexpr uint32_t SPV_VERSION_1_1 = (1u << 16) | (1u << 8);
    constexpr uint32_t SPV_VERSION_1_2 = (1u << 16) | (2u << 8);
    constexpr uint32_t SPV_VERSION_1_3 = (1u << 16) | (3u << 8);
    constexpr uint32_t SPV_VERSION_1_4 = (1u << 16) | (4u << 8);
    constexpr uint32_t SPV_VERSION_1_5 = (1u << 16) | (5u << 8);
    constexpr uint32_t SPV_VERSION_1_6 = (1u << 16) | (6u << 8);

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

    inline void ResolveShaderTargetVersionsFromApi(const uint32_t api_version,
                                                   const bool has_spirv_1_4_extension,
                                                   uint32_t &out_vulkan_version,
                                                   uint32_t &out_spv_version)
    {
        out_vulkan_version = MakeVkVersion(1, 0);
        out_spv_version = SPV_VERSION_1_0;

        if (api_version >= MakeVkVersion(1, 3))
        {
            out_vulkan_version = MakeVkVersion(1, 3);
            out_spv_version = SPV_VERSION_1_6;
            return;
        }

        if (api_version >= MakeVkVersion(1, 2))
        {
            out_vulkan_version = MakeVkVersion(1, 2);
            out_spv_version = SPV_VERSION_1_5;
            return;
        }

        if (api_version >= MakeVkVersion(1, 1))
        {
            out_vulkan_version = MakeVkVersion(1, 1);
            out_spv_version = has_spirv_1_4_extension ? SPV_VERSION_1_4 : SPV_VERSION_1_3;
        }
    }

    inline void ResolveShaderTargetVersions(const PhysicalDeviceProfileLite &profile,
                                            uint32_t &out_vulkan_version,
                                            uint32_t &out_spv_version)
    {
        const uint32_t api_or_target = profile.target_vulkan_version != 0
                                      ? profile.target_vulkan_version
                                      : profile.api_version;

        ResolveShaderTargetVersionsFromApi(api_or_target, false, out_vulkan_version, out_spv_version);

        if (profile.target_vulkan_version != 0)
            out_vulkan_version = profile.target_vulkan_version;

        if (profile.target_spv_version != 0)
            out_spv_version = profile.target_spv_version;
    }
}
