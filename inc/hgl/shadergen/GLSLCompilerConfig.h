#pragma once

#include <hgl/type/DataType.h>
#include <hgl/shadergen/device/DeviceProfile.h>
#include <string>

namespace hgl::graph
{
    struct ShaderCompilerContext
    {
        std::string shader_library_path;
        bool has_profile = false;
        mtl::contract::PhysicalDeviceProfileLite profile{};
    };

    void SetShaderCompilerPhysicalDeviceProfile(const mtl::contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);

    // Phase 5 explicit context API.
    // Legacy setters remain available for compatibility.
    void ApplyShaderCompilerContext(const ShaderCompilerContext &context);
    ShaderCompilerContext CaptureShaderCompilerContext();

    /// 添加 GLSL #include 搜索路径（在 InitShaderCompiler 之后调用）
    void AddShaderIncludePath(const char *path);
}
