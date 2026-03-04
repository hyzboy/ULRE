#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph
{
    struct VulkanDevAttr;
    class VulkanPhyDevice;

    struct ShaderCompilerCompileContext
    {
        const VulkanDevAttr *dev_attr = nullptr;
        const mtl::contract::PhysicalDeviceProfileLite *profile = nullptr;
    };

    void PushShaderCompilerCompileContext(const VulkanDevAttr *dev_attr,
                                          const mtl::contract::PhysicalDeviceProfileLite *profile);
    void PopShaderCompilerCompileContext();
    bool GetShaderCompilerCompileContext(ShaderCompilerCompileContext &out_context);

    class ScopedShaderCompilerCompileContext
    {
    public:
        ScopedShaderCompilerCompileContext(const VulkanDevAttr *dev_attr,
                                           const mtl::contract::PhysicalDeviceProfileLite *profile)
        {
            PushShaderCompilerCompileContext(dev_attr, profile);
        }

        ~ScopedShaderCompilerCompileContext()
        {
            PopShaderCompilerCompileContext();
        }
    };

    // Preferred runtime entry (profile-first).
    void SetShaderCompilerPhysicalDeviceProfileFromRuntimeDevice(const VulkanPhyDevice *pd);

    void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);
}
