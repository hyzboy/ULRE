/**
 * ShaderProgramManifestBackend.h —— 发行形态的 `ShaderProgramBuildBackend` 实现。
 *
 * 三件事：
 *   · `EnsureShaderProgramManifestBackend()` —— 当前没有后端时装入 Manifest 后端
 *     （开发形态里 `InitShaderCompiler()` 早已装好生成器后端，这里是个空操作）；
 *   · `RecordShaderProgramManifestEntry()` —— 烘焙/开发形态把「语义键 → program
 *     link」写进清单，让下一次发行形态能查到；
 *   · 清单查不到一律**硬失败**（`CreateBuildContext` 返回 nullptr），绝不回落到
 *     编译器——发行形态本来就没有编译器，回落到「凑一份」只会掩盖烘焙缺漏。
 */
#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>

namespace hgl::graph::mtl
{
    class ShaderArtifactStore;
    struct ShaderProgramBuildBackend;

    /// 若当前没有构建后端，尝试装入 Manifest 后端。
    /// 返回 true = 现在有可用后端（可能是已存在的生成器后端）。
    bool EnsureShaderProgramManifestBackend(ShaderArtifactStore *store);

    /// 卸下 Manifest 后端（仅当当前后端确实是它时生效）。
    void UninstallShaderProgramManifestBackend();

    /// 记录一条「语义键 → program link」。`ReadOnly` 缓存模式或清单不可写时返回 false。
    bool RecordShaderProgramManifestEntry(
        ShaderArtifactStore *store,
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderProgramManifestEntry &entry);

    /// 已装载清单条目数（诊断/回归门用）；未装载返回 0。
    uint32 GetShaderProgramManifestEntryCount();
}//namespace hgl::graph::mtl
