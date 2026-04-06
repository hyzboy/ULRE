#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderGenValidationReport
    {
        bool overall_valid = true;
        bool diff_valid = true;
        bool result_valid = true;
        bool request_result_valid = true;

        std::string category;

        uint32_t warning_count = 0;
        uint32_t error_count = 0;

        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct ShaderGenValidationReportRecord
    {
        uint64_t sequence = 0;
        std::string material_name;
        ShaderGenValidationReport report;
    };

    struct ShaderGenProfilerSnapshot
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
        std::map<std::string, uint64_t> contract_path_decision_histogram;
    };
}
