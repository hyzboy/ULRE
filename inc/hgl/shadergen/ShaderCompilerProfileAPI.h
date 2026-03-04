#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph
{
    class VulkanPhyDevice;

    void SetShaderCompilerVersion(const VulkanPhyDevice *pd);

    void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);
}
