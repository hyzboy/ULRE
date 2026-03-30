#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/device/DeviceProfile.h>

namespace hgl::graph
{
    void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);

    /// 添加 GLSL #include 搜索路径（在 InitShaderCompiler 之后调用）
    void AddShaderIncludePath(const char *path);
}
