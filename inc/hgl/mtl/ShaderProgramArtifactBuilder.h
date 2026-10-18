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

    /// generated_source_digest 的唯一算法真源（发射后的 GLSL 文本派生）。
    ///
    /// 落盘与命中后校验必须走同一个函数，否则校验本身没有意义。
    /// 注意它**不**参与缓存命中判定（见 ShaderArtifactContract.h 的
    /// operator==），只用于命中后确认落盘 SPV 与当前文本仍然一致。
    uint64 ComputeGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size) noexcept;

    /// 命中后校验的唯一实现：落盘摘要是否仍与当前生成的文本一致。
    ///
    /// 与 ComputeGeneratedSourceDigest 分开暴露，是为了让判定语义可被
    /// 回归门禁直接断言，而不是只能靠跑完整编译去观察日志。
    /// 返回 false 表示「声明键欠近似」，调用方必须丢弃缓存重编译。
    bool MatchesGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size,
        const uint64 stored_source_digest) noexcept;
}
