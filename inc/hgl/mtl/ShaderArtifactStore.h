#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/filesystem/Path.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    class ShaderArtifactStore
    {
        OSString root_path;
        ShaderCacheMode cache_mode = ShaderCacheMode::BuildIfMissing;

        OSString GetStagePath(const ShaderStageKey &key) const;
        OSString GetProgramPath(const ShaderProgramKey &key) const;

    public:
        ShaderArtifactStore() = default;
        ShaderArtifactStore(
            const OSString &root,
            const ShaderCacheMode mode)
            : root_path(root),
              cache_mode(mode) {}

        ShaderCacheMode GetCacheMode() const noexcept { return cache_mode; }

        /// Each stage SPV carries its own source digest and can be loaded independently.
        bool LoadStageSPV(
            const ShaderStageKey &key,
            ValueArray<uint8> &out_spv,
            uint64 *out_source_digest = nullptr) const;
        bool SaveStageSPV(
            const ShaderStageKey &key,
            const void *spv_data,
            const uint64 spv_size,
            const uint64 source_digest = 0);

        /// 把生成该 stage SPV 的最终 GLSL 源以纯文本落盘：
        /// 与 stage SPV 缓存文件同样的主文件名（同目录，扩展名按 stage 定
        /// ——.mesh/.frag，见 ShaderArtifactContract.h），便于日后对照分析。
        /// Best-effort，失败只记日志不阻断 shader 缓存。
        bool SaveStageGLSL(const ShaderStageKey &key, const void *glsl_text, const uint64 byte_size);

        bool HasProgramMetadata(
            const ShaderLinkSpec &link) const;
        bool LoadProgramMetadata(
            const ShaderLinkSpec &link,
            ShaderProgramArtifactMetadata &out_metadata) const;
        bool SaveProgramMetadata(
            const ShaderLinkSpec &link,
            const ShaderProgramArtifactMetadata &metadata);

        /// Compatibility helper for callers that still require an all-stage program load.
        bool LoadProgramArtifacts(
            const ShaderLinkSpec &link,
            const ShaderProgramArtifactMetadata &expected_metadata,
            ValueArray<uint8> &out_vertex_spv,
            ValueArray<uint8> &out_fragment_spv,
            uint64 *out_stored_source_digest = nullptr) const;
    };
}
