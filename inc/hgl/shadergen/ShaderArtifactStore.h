#pragma once

#include <hgl/CoreType.h>
#include <hgl/filesystem/Path.h>
#include <hgl/shadergen/ShaderArtifactContract.h>
#include <hgl/shadergen/ShaderStageKey.h>
#include <hgl/shadergen/ShaderProgramLinkSpec.h>
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

        const OSString &GetRootPath() const noexcept { return root_path; }
        ShaderCacheMode GetCacheMode() const noexcept { return cache_mode; }
        void SetCacheMode(const ShaderCacheMode mode) noexcept { cache_mode = mode; }

        bool HasStageSPV(const ShaderStageKey &key) const;
        bool LoadStageSPV(const ShaderStageKey &key, ValueArray<uint8> &out_spv) const;
        bool SaveStageSPV(const ShaderStageKey &key, const void *spv_data, const uint64 spv_size);

        bool HasProgramMetadata(
            const ShaderProgramLinkSpec &link) const;
        bool LoadProgramMetadata(
            const ShaderProgramLinkSpec &link,
            ShaderProgramArtifactMetadata &out_metadata) const;
        bool SaveProgramMetadata(
            const ShaderProgramLinkSpec &link,
            const ShaderProgramArtifactMetadata &metadata);

        bool LoadProgramArtifacts(
            const ShaderProgramLinkSpec &link,
            const ShaderProgramArtifactMetadata &expected_metadata,
            ValueArray<uint8> &out_vertex_spv,
            ValueArray<uint8> &out_fragment_spv) const;
    };
}
