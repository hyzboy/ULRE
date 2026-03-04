#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <type_traits>

namespace hgl::graph
{
    class VulkanPhyDevice;

    // Preferred runtime entry (profile-first).
    void SetShaderCompilerPhysicalDeviceProfileFromRuntimeDevice(const VulkanPhyDevice *pd);

    // Legacy path hard-stop: any call to this API triggers compile-time failure.
    template<typename LegacyPathGuard = void>
    inline void SetShaderCompilerVersion(const VulkanPhyDevice *)
    {
        static_assert(!std::is_same_v<LegacyPathGuard, LegacyPathGuard>,
                      "Legacy API SetShaderCompilerVersion(...) is blocked. Use SetShaderCompilerPhysicalDeviceProfileFromRuntimeDevice(...) instead.");
    }

    void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);

    // Returns how many times legacy compatibility API SetShaderCompilerVersion(...) was called.
    // In release builds this value is always 0.
    uint32 GetShaderCompilerLegacyVersionApiCallCount();
}
