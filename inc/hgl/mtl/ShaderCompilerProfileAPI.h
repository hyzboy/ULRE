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
    /// carries `#include "generated/frag_options.glsl"`, while that generated file lives in
    /// the build cache, not in the source tree — the two must be visible at once.
    /// All-or-nothing: if any root fails to resolve, the previous configuration stays.
    bool SetShaderIncludeRoots(const char **paths, uint32_t count);
    /// Return the configured root, resolving the default ShaderLibrary location on first use.
    OSString GetShaderIncludeRoot();
    /// Number of roots currently configured (0 before any configuration).
    uint32_t GetShaderIncludeRootCount();

    /// 编译上下文锁。include 根表与 `compile_info` 是进程级全局，且
    /// `SetShaderIncludeRoots -> 编译 -> 恢复根` 是一个跨调用序列：任何执行该
    /// 切换序列的运行时路径必须整体持锁（RAII 包裹），否则并发编译互相污染根表。
    /// 锁不可重入：`SetShaderIncludeRoots`/`CompileShader` 内部不加此锁，
    /// 持锁责任在序列的发起方；初始化期的单线程配置无需加锁。
    void LockShaderCompileContext();
    void UnlockShaderCompileContext();
}
