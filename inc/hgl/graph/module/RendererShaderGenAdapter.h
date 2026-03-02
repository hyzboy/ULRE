#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <cstdint>
#include <map>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;
    }

    class RendererShaderGenAdapter
    {
    public:

        struct ProfilerSnapshot
        {
            uint64_t sample_count = 0;

            uint64_t all_match_count = 0;
            uint64_t layout_match_count = 0;
            uint64_t vertex_match_count = 0;
            uint64_t spv_match_count = 0;

            uint64_t legacy_layout_count_sum = 0;
            uint64_t mirror_layout_count_sum = 0;
            uint64_t legacy_vertex_count_sum = 0;
            uint64_t mirror_vertex_count_sum = 0;
            uint64_t legacy_spv_count_sum = 0;
            uint64_t mirror_spv_count_sum = 0;

            std::map<uint32_t, uint64_t> legacy_stage_combo_histogram;
            std::map<uint32_t, uint64_t> mirror_stage_combo_histogram;
        };

        enum class DiffLogDetail
        {
            SummaryOnly,
            Full,
        };

        static void ResetProfiler();
        static ProfilerSnapshot GetProfilerSnapshot();

        bool ConsumePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail = DiffLogDetail::Full) const;
        bool ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        bool ConsumeMaterialReadOnly(const mtl::MaterialCreateInfo &mci, const char *material_name, DiffLogDetail detail = DiffLogDetail::Full) const;
    };
}//namespace hgl::graph
