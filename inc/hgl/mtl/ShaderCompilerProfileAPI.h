#pragma once

namespace hgl::graph::mtl {}

#include <hgl/type/DataType.h>
#include <hgl/mtl/contract/ShaderGenContract.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    void SetShaderCompilerPhysicalDeviceProfile(const contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);
    void GetShaderCompilerTargetVersions(uint32 &vulkan_version, uint32 &spv_version);

    /// 添加 GLSL #include 搜索路径（在 InitShaderCompiler 之后调用）
    void AddShaderIncludePath(const char *path);
}
