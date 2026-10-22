#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/ShaderArtifactContract.h>

#include <cstddef>

namespace hgl::graph::mtl
{
    namespace contract
    {
        struct PhysicalDeviceProfileLite;
    }

    class ShaderBuildContext;

    bool BuildShaderProgramArtifactMetadata(
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderBuildContext &build_spec,
        ShaderProgramArtifactMetadata &out_metadata) noexcept;

    /// Program metadata 中 combined generated_source_digest 的算法真源。
    ///
    /// 落盘与命中后校验必须走同一个函数，否则校验本身没有意义。
    /// Stage SPV 的独立校验使用 ComputeGeneratedStageSourceDigest。
    uint64 ComputeGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size) noexcept;

    uint64 ComputeGeneratedStageSourceDigest(
        const char *glsl,
        const size_t glsl_size) noexcept;

    /// Combined program metadata 摘要校验 helper，保留给兼容调用方/回归使用。
    ///
    /// 与 ComputeGeneratedSourceDigest 分开暴露，是为了让判定语义可被
    /// 回归门禁直接断言，而不是只能靠跑完整编译去观察日志。
    /// 返回 false 表示摘要不匹配。
    bool MatchesGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size,
        const uint64 stored_source_digest) noexcept;
}
