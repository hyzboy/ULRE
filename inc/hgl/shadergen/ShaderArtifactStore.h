#pragma once

#include <hgl/CoreType.h>
#include <hgl/filesystem/Path.h>
#include <hgl/shadergen/ShaderArtifactContract.h>
#include <hgl/shadergen/ShaderStageKey.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    class ShaderArtifactStore
    {
        OSString root_path;
        ShaderCacheMode cache_mode = ShaderCacheMode::BuildIfMissing;

        OSString GetStagePath(const ShaderStageKey &key) const;

    public:
        ShaderArtifactStore() = default;
        ShaderArtifactStore(const OSString &root, const ShaderCacheMode mode)
            : root_path(root), cache_mode(mode) {}

        const OSString &GetRootPath() const noexcept { return root_path; }
        ShaderCacheMode GetCacheMode() const noexcept { return cache_mode; }
        void SetCacheMode(const ShaderCacheMode mode) noexcept { cache_mode = mode; }

        bool HasStageSPV(const ShaderStageKey &key) const;
        bool LoadStageSPV(const ShaderStageKey &key, ValueArray<uint8> &out_spv) const;
        bool SaveStageSPV(const ShaderStageKey &key, const void *spv_data, const uint64 spv_size);
    };
}
