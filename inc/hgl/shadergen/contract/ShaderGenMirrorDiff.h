#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <cstdint>
#include <string>

namespace hgl::graph::mtl
{
    class MaterialCreateInfo;
}

namespace hgl::graph::mtl::contract
{
    struct ShaderGenMirrorDiffSummary
    {
        bool all_match = false;
        bool layout_match = false;
        bool vertex_match = false;
        bool spv_match = false;

        uint64_t layout_hash_legacy = 0;
        uint64_t layout_hash_mirror = 0;
        uint64_t vertex_hash_legacy = 0;
        uint64_t vertex_hash_mirror = 0;
        uint64_t spv_hash_legacy = 0;
        uint64_t spv_hash_mirror = 0;

        uint32_t legacy_layout_count = 0;
        uint32_t mirror_layout_count = 0;
        uint32_t legacy_vertex_count = 0;
        uint32_t mirror_vertex_count = 0;
        uint32_t legacy_spv_count = 0;
        uint32_t mirror_spv_count = 0;

        uint32_t legacy_stage_combo = 0;
        uint32_t mirror_stage_combo = 0;

        std::string legacy_stage_summary;
        std::string mirror_stage_summary;
    };

    bool BuildShaderGenMirrorDiffSummary(const MaterialCreateInfo &mci,
                                         const ShaderGenResult &result,
                                         ShaderGenMirrorDiffSummary &summary);
}
