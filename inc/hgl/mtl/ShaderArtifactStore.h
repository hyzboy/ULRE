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
    using namespace hgl::graph::mtl;
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

        const OSString &GetRootPath() const noexcept { return root_path; }
        ShaderCacheMode GetCacheMode() const noexcept { return cache_mode; }
        void SetCacheMode(const ShaderCacheMode mode) noexcept { cache_mode = mode; }

        bool LoadStageSPV(const ShaderStageKey &key, ValueArray<uint8> &out_spv) const;
        bool SaveStageSPV(const ShaderStageKey &key, const void *spv_data, const uint64 spv_size);

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

        bool LoadProgramArtifacts(
            const ShaderLinkSpec &link,
            const ShaderProgramArtifactMetadata &expected_metadata,
            ValueArray<uint8> &out_vertex_spv,
            ValueArray<uint8> &out_fragment_spv) const;
    };
}
