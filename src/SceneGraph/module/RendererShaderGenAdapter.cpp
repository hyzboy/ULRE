#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenMirrorDiff.h>
#include <hgl/shadergen/contract/ShaderGenContractValidator.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <deque>

namespace hgl::graph
{
    namespace
    {
        struct ShaderGenProfilerStorage
        {
            RendererShaderGenAdapter::ProfilerSnapshot snapshot;
            RendererShaderGenAdapter::ValidationReport last_report;
            std::string last_report_material_name;
            bool has_last_report = false;
            uint64_t report_sequence = 0;
            std::deque<RendererShaderGenAdapter::ValidationReportRecord> history_reports;
            std::mutex mutex;
        };

        ShaderGenProfilerStorage &GetShaderGenProfilerStorage()
        {
            static ShaderGenProfilerStorage storage;
            return storage;
        }

        void AddWarning(RendererShaderGenAdapter::ValidationReport &report, const std::string &message)
        {
            report.warnings.emplace_back(message);
            ++report.warning_count;
        }

        void AddError(RendererShaderGenAdapter::ValidationReport &report, const std::string &message)
        {
            report.errors.emplace_back(message);
            ++report.error_count;
            report.overall_valid = false;
        }

        void StoreValidationReport(const char *material_name, const RendererShaderGenAdapter::ValidationReport &report)
        {
            auto &storage = GetShaderGenProfilerStorage();
            std::lock_guard<std::mutex> lock(storage.mutex);

            const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

            storage.last_report = report;
            storage.last_report_material_name = mat_name;
            storage.has_last_report = true;

            RendererShaderGenAdapter::ValidationReportRecord rec;
            rec.sequence = ++storage.report_sequence;
            rec.material_name = mat_name;
            rec.report = report;

            storage.history_reports.emplace_back(std::move(rec));

            constexpr size_t kMaxValidationHistory = 512;
            while (storage.history_reports.size() > kMaxValidationHistory)
                storage.history_reports.pop_front();
        }

        void MergeValidationReport(RendererShaderGenAdapter::ValidationReport &dst, const RendererShaderGenAdapter::ValidationReport &src)
        {
            dst.overall_valid = dst.overall_valid && src.overall_valid;
            dst.warning_count += src.warning_count;
            dst.error_count += src.error_count;

            dst.warnings.insert(dst.warnings.end(), src.warnings.begin(), src.warnings.end());
            dst.errors.insert(dst.errors.end(), src.errors.begin(), src.errors.end());
        }
    }//namespace

    static int BoolToInt(const bool v)
    {
        return v ? 1 : 0;
    }
    static void RecordProfilerSample(const bool all_match,
                                     const bool layout_match,
                                     const bool vertex_match,
                                     const bool spv_match,
                                     const uint32_t legacy_stage_combo,
                                     const uint32_t mirror_stage_combo,
                                     const size_t legacy_layout_count,
                                     const size_t mirror_layout_count,
                                     const size_t legacy_vertex_count,
                                     const size_t mirror_vertex_count,
                                     const size_t legacy_spv_count,
                                     const size_t mirror_spv_count)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        auto &snapshot = storage.snapshot;
        ++snapshot.sample_count;

        if (all_match) ++snapshot.all_match_count;
        if (layout_match) ++snapshot.layout_match_count;
        if (vertex_match) ++snapshot.vertex_match_count;
        if (spv_match) ++snapshot.spv_match_count;

        snapshot.legacy_layout_count_sum += static_cast<uint64_t>(legacy_layout_count);
        snapshot.mirror_layout_count_sum += static_cast<uint64_t>(mirror_layout_count);
        snapshot.legacy_vertex_count_sum += static_cast<uint64_t>(legacy_vertex_count);
        snapshot.mirror_vertex_count_sum += static_cast<uint64_t>(mirror_vertex_count);
        snapshot.legacy_spv_count_sum += static_cast<uint64_t>(legacy_spv_count);
        snapshot.mirror_spv_count_sum += static_cast<uint64_t>(mirror_spv_count);

        ++snapshot.legacy_stage_combo_histogram[legacy_stage_combo];
        ++snapshot.mirror_stage_combo_histogram[mirror_stage_combo];
    }

    static bool PrintLegacyMirrorDiff(const mtl::MaterialCreateInfo &mci,
                                      const mtl::contract::ShaderGenResult &result,
                                      const char *material_name,
                                      const RendererShaderGenAdapter::DiffLogDetail detail,
                                      RendererShaderGenAdapter::ValidationReport *out_report)
    {
        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        mtl::contract::ShaderGenMirrorDiffSummary diff_summary;
        mtl::contract::BuildShaderGenMirrorDiffSummary(mci, result, diff_summary);

        RecordProfilerSample(diff_summary.all_match,
                     diff_summary.layout_match,
                     diff_summary.vertex_match,
                     diff_summary.spv_match,
                     diff_summary.legacy_stage_combo,
                     diff_summary.mirror_stage_combo,
                     diff_summary.legacy_layout_count,
                     diff_summary.mirror_layout_count,
                     diff_summary.legacy_vertex_count,
                     diff_summary.mirror_vertex_count,
                     diff_summary.legacy_spv_count,
                     diff_summary.mirror_spv_count);

        if(detail == RendererShaderGenAdapter::DiffLogDetail::Full)
        {
            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=layout legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_layout_count),
                static_cast<unsigned>(diff_summary.mirror_layout_count),
                static_cast<unsigned long long>(diff_summary.layout_hash_legacy),
                static_cast<unsigned long long>(diff_summary.layout_hash_mirror),
                BoolToInt(diff_summary.layout_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=vertex legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_vertex_count),
                static_cast<unsigned>(diff_summary.mirror_vertex_count),
                static_cast<unsigned long long>(diff_summary.vertex_hash_legacy),
                static_cast<unsigned long long>(diff_summary.vertex_hash_mirror),
                BoolToInt(diff_summary.vertex_match));

            std::fprintf(stderr,
                "[RendererShaderGenAdapter][DiffKV] material=%s event=spv legacy_count=%u mirror_count=%u legacy_hash=0x%llx mirror_hash=0x%llx legacy_stages=%s mirror_stages=%s match=%d\n",
                mat_name,
                static_cast<unsigned>(diff_summary.legacy_spv_count),
                static_cast<unsigned>(diff_summary.mirror_spv_count),
                static_cast<unsigned long long>(diff_summary.spv_hash_legacy),
                static_cast<unsigned long long>(diff_summary.spv_hash_mirror),
                diff_summary.legacy_stage_summary.c_str(),
                diff_summary.mirror_stage_summary.c_str(),
                BoolToInt(diff_summary.spv_match));
        }

        if (!diff_summary.all_match && out_report)
        {
            char msg[512] = {};
            std::snprintf(msg,
                sizeof(msg),
                "material=%s legacy/mirror diff mismatch: layout=%d vertex=%d spv=%d legacy_stages=%s mirror_stages=%s",
                mat_name,
                BoolToInt(diff_summary.layout_match),
                BoolToInt(diff_summary.vertex_match),
                BoolToInt(diff_summary.spv_match),
                diff_summary.legacy_stage_summary.c_str(),
                diff_summary.mirror_stage_summary.c_str());
            AddError(*out_report, msg);
            out_report->diff_valid = false;
        }

        return diff_summary.all_match;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidateResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        ValidationReport report;
        report.result_valid = true;

        const auto contract_check = mtl::contract::ValidateShaderGenResult(result, material_name);
        report.result_valid = contract_check.valid;

        report.warning_count += contract_check.warning_count;
        report.error_count += contract_check.error_count;
        report.warnings.insert(report.warnings.end(), contract_check.warnings.begin(), contract_check.warnings.end());
        report.errors.insert(report.errors.end(), contract_check.errors.begin(), contract_check.errors.end());

        if (!contract_check.valid)
            report.overall_valid = false;

        return report;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidatePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail) const
    {
        ValidationReport report;
        report.diff_valid = true;
        report.result_valid = true;

        const bool diff_ok = PrintLegacyMirrorDiff(mci, result, material_name, detail, &report);
        if (!diff_ok)
            report.diff_valid = false;

        const ValidationReport result_report = ValidateResultReadOnly(result, material_name);
        report.result_valid = result_report.result_valid;
        MergeValidationReport(report, result_report);

        report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;

        StoreValidationReport(material_name, report);
        return report;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidateRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        ValidationReport report;
        report.request_result_valid = true;

        const auto contract_check = mtl::contract::ValidateShaderGenRequestResult(request, result, material_name);
        report.request_result_valid = contract_check.valid;

        report.warning_count += contract_check.warning_count;
        report.error_count += contract_check.error_count;
        report.warnings.insert(report.warnings.end(), contract_check.warnings.begin(), contract_check.warnings.end());
        report.errors.insert(report.errors.end(), contract_check.errors.begin(), contract_check.errors.end());

        if (!contract_check.valid)
            report.overall_valid = false;

        report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;
        StoreValidationReport(material_name, report);
        return report;
    }

    RendererShaderGenAdapter::ValidationReport RendererShaderGenAdapter::ValidateMaterialContractReadOnly(const mtl::MaterialCreateInfo &mci,
                                                                                                           const mtl::contract::ShaderGenRequest *request,
                                                                                                           const mtl::contract::ShaderGenResult *result,
                                                                                                           const char *material_name,
                                                                                                           DiffLogDetail detail) const
    {
        ValidationReport report;

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        if (result)
        {
            report = ValidatePairReadOnly(mci, *result, mat_name, detail);

            if (request)
            {
                const ValidationReport req_report = ValidateRequestResultReadOnly(*request, *result, mat_name);

                report.request_result_valid = req_report.request_result_valid;
                MergeValidationReport(report, req_report);
                report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;

                StoreValidationReport(mat_name, report);
            }

            return report;
        }

        mtl::contract::ShaderGenResult built_result;
        if (!mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(mci, built_result))
        {
            char msg[256] = {};
            std::snprintf(msg,
                          sizeof(msg),
                          "material=%s failed to build mirror result",
                          mat_name);
            AddError(report, msg);
            report.diff_valid = false;
            report.result_valid = false;
            report.request_result_valid = (request == nullptr);
            report.overall_valid = false;
            StoreValidationReport(mat_name, report);
            return report;
        }

        report = ValidatePairReadOnly(mci, built_result, mat_name, detail);

        if (request)
        {
            const ValidationReport req_report = ValidateRequestResultReadOnly(*request, built_result, mat_name);

            report.request_result_valid = req_report.request_result_valid;
            MergeValidationReport(report, req_report);
            report.overall_valid = report.diff_valid && report.result_valid && report.request_result_valid && report.error_count == 0;

            StoreValidationReport(mat_name, report);
        }

        return report;
    }

    bool RendererShaderGenAdapter::ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const ValidationReport report = ValidateResultReadOnly(result, material_name);
        StoreValidationReport(material_name, report);
        return report.overall_valid;
    }

    bool RendererShaderGenAdapter::ConsumePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail) const
    {
        const ValidationReport report = ValidatePairReadOnly(mci, result, material_name, detail);
        return report.overall_valid;
    }

    bool RendererShaderGenAdapter::ConsumeRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        const ValidationReport report = ValidateRequestResultReadOnly(request, result, material_name);
        return report.overall_valid;
    }

    void RendererShaderGenAdapter::ResetProfiler()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        storage.snapshot = ProfilerSnapshot{};
    }

    RendererShaderGenAdapter::ProfilerSnapshot RendererShaderGenAdapter::GetProfilerSnapshot()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        return storage.snapshot;
    }

    bool RendererShaderGenAdapter::GetLastValidationReport(ValidationReport &out_report, std::string *out_material_name)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        if (!storage.has_last_report)
            return false;

        out_report = storage.last_report;

        if (out_material_name)
            *out_material_name = storage.last_report_material_name;

        return true;
    }

    std::vector<RendererShaderGenAdapter::ValidationReportRecord> RendererShaderGenAdapter::GetRecentValidationReports(uint32_t max_count)
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        std::vector<ValidationReportRecord> out;
        if (max_count == 0 || storage.history_reports.empty())
            return out;

        const size_t take = std::min<size_t>(max_count, storage.history_reports.size());
        out.reserve(take);

        auto it = storage.history_reports.rbegin();
        for (size_t i = 0; i < take && it != storage.history_reports.rend(); ++i, ++it)
            out.emplace_back(*it);

        return out;
    }

    std::map<std::string, std::vector<RendererShaderGenAdapter::ValidationReportRecord>> RendererShaderGenAdapter::GetRecentValidationReportsByMaterial(uint32_t max_per_material, uint32_t max_total)
    {
        std::map<std::string, std::vector<ValidationReportRecord>> grouped;

        if (max_per_material == 0 || max_total == 0)
            return grouped;

        const auto recent = GetRecentValidationReports(max_total);

        for (const auto &record : recent)
        {
            auto &bucket = grouped[record.material_name];
            if (bucket.size() >= max_per_material)
                continue;

            bucket.emplace_back(record);
        }

        return grouped;
    }

    bool RendererShaderGenAdapter::ConsumeMaterialReadOnly(const mtl::MaterialCreateInfo &mci, const char *material_name, DiffLogDetail detail) const
    {
        mtl::contract::ShaderGenResult result;
        if (!mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(mci, result))
        {
            const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";
            std::fprintf(stderr,
                "[RendererShaderGenAdapter] material=%s failed to build mirror result\n",
                mat_name);
            return false;
        }

        return ConsumePairReadOnly(mci, result, material_name, detail);
    }
}//namespace hgl::graph
