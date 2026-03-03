#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;
    }

    class RendererShaderGenAdapter
    {
    public:

        struct ValidationReport
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

        struct ValidationReportRecord
        {
            uint64_t sequence = 0;
            std::string material_name;
            ValidationReport report;
        };

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
        static void ResetValidationReports();
        static ProfilerSnapshot GetProfilerSnapshot();
        static bool GetLastValidationReport(ValidationReport &out_report, std::string *out_material_name = nullptr);
        static std::vector<ValidationReportRecord> GetRecentValidationReports(uint32_t max_count = 64);
        static std::map<std::string, std::vector<ValidationReportRecord>> GetRecentValidationReportsByMaterial(uint32_t max_per_material = 4, uint32_t max_total = 128);
        static std::map<std::string, uint32_t> GetRecentValidationReportCategoryHistogram(uint32_t max_count = 128);
        static void RecordExternalValidationError(const char *material_name, const char *message, const char *category = nullptr);

        ValidationReport ValidateMaterialContractReadOnly(const mtl::MaterialCreateInfo &mci,
                                 const mtl::contract::ShaderGenRequest *request,
                                 const mtl::contract::ShaderGenResult *result,
                                 const char *material_name,
                                 DiffLogDetail detail = DiffLogDetail::Full) const;

    private:
        ValidationReport ValidateRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        ValidationReport ValidateResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        ValidationReport ValidatePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail = DiffLogDetail::Full) const;

    };
}//namespace hgl::graph
