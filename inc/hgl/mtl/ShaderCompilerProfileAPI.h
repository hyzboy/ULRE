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
    /// Configure the include root table: `paths[0]` is the primary root (same role as
    /// `SetShaderIncludeRoot`), the rest are additional roots searched in order.
    ///
    /// Needed because a physically fixed master shader (`ShaderLibrary/domain/*.frag`)
    /// carries `#include "generated/options.glsl"`, while that generated file lives in
    /// the build cache, not in the source tree — the two must be visible at once.
    /// All-or-nothing: if any root fails to resolve, the previous configuration stays.
    bool SetShaderIncludeRoots(const char **paths, uint32_t count);
    /// Return the configured root, resolving the default ShaderLibrary location on first use.
    OSString GetShaderIncludeRoot();
    /// Number of roots currently configured (0 before any configuration).
    uint32_t GetShaderIncludeRootCount();
}
