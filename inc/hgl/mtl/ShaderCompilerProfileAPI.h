#pragma once

namespace hgl::graph::mtl {}

#include <hgl/type/DataType.h>
#include <hgl/type/String.h>
#include <hgl/mtl/contract/ShaderGenContract.h>

namespace hgl::graph::mtl
{
    void SetShaderCompilerPhysicalDeviceProfile(const contract::PhysicalDeviceProfileLite &profile);
    bool SetShaderCompilerPhysicalDeviceProfileFromJson(const char *json_text);

    /// Configure one include root before generating/compiling shaders; never adds a fallback.
    bool SetShaderIncludeRoot(const char *path);
    /// Return the configured root, resolving the default ShaderLibrary location on first use.
    OSString GetShaderIncludeRoot();
}
