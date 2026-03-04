#include <hgl/graph/module/ShaderGenValidationStorageService.h>
#include <hgl/graph/module/ShaderGenValidationReportUtils.h>
#include <algorithm>
#include <deque>
#include <mutex>

namespace hgl::graph
{
    namespace
    {
        struct ShaderGenProfilerStorage
        {
            ShaderGenProfilerSnapshot snapshot;
            std::mutex mutex;
        };

        struct ShaderGenValidationReportStorage
        {
            ShaderGenValidationReport last_report;
            std::string last_report_material_name;
            bool has_last_report = false;
            uint64_t report_sequence = 0;
            std::deque<ShaderGenValidationReportRecord> history_reports;
            std::mutex mutex;
        };

        ShaderGenProfilerStorage &GetShaderGenProfilerStorage()
        {
            static ShaderGenProfilerStorage storage;
            return storage;
        }

        ShaderGenValidationReportStorage &GetShaderGenValidationReportStorage()
        {
            static ShaderGenValidationReportStorage storage;
            return storage;
        }

        std::map<std::string, std::vector<ShaderGenValidationReportRecord>> BuildRecentValidationReportsByMaterial(
            const std::vector<ShaderGenValidationReportRecord> &recent,
            uint32_t max_per_material)
        {
            std::map<std::string, std::vector<ShaderGenValidationReportRecord>> grouped;

            if (max_per_material == 0)
                return grouped;

            for (const auto &record : recent)
            {
                auto &bucket = grouped[record.material_name];
                if (bucket.size() >= max_per_material)
                    continue;

                bucket.emplace_back(record);
            }

            return grouped;
        }
    }

    void RecordShaderGenProfilerSample(bool all_match,
                                       bool layout_match,
                                       bool vertex_match,
                                       bool spv_match,
                                       uint32_t legacy_stage_combo,
                                       uint32_t mirror_stage_combo,
                                       size_t legacy_layout_count,
                                       size_t mirror_layout_count,
                                       size_t legacy_vertex_count,
                                       size_t mirror_vertex_count,
                                       size_t legacy_spv_count,
                                       size_t mirror_spv_count)
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

    void RecordShaderGenContractPathDecision(const char *decision_key)
    {
        if (!decision_key || !decision_key[0])
            return;

        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        ++storage.snapshot.contract_path_decision_histogram[decision_key];
    }

    void StoreShaderGenValidationReport(const char *material_name,
                                        const ShaderGenValidationReport &report)
    {
        auto &storage = GetShaderGenValidationReportStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        const char *mat_name = (material_name && material_name[0]) ? material_name : "<unnamed-material>";

        storage.last_report = report;
        storage.last_report_material_name = mat_name;
        storage.has_last_report = true;

        ShaderGenValidationReportRecord rec;
        rec.sequence = ++storage.report_sequence;
        rec.material_name = mat_name;
        rec.report = report;

        storage.history_reports.emplace_back(std::move(rec));

        constexpr size_t kMaxValidationHistory = 512;
        while (storage.history_reports.size() > kMaxValidationHistory)
            storage.history_reports.pop_front();
    }

    void StoreExternalShaderGenValidationError(const char *material_name,
                                               const char *message,
                                               const char *category)
    {
        ShaderGenValidationReport report;
        report.overall_valid = false;
        report.diff_valid = false;
        report.result_valid = false;
        report.request_result_valid = false;
        report.category = (category && category[0]) ? category : "External";

        const char *msg = (message && message[0]) ? message : "external validation error";
        AddShaderGenValidationError(report, msg);

        StoreShaderGenValidationReport(material_name, report);
    }

    void ResetShaderGenProfilerStorage()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        storage.snapshot = ShaderGenProfilerSnapshot{};
    }

    void ResetShaderGenValidationReportStorage()
    {
        auto &storage = GetShaderGenValidationReportStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        storage.last_report = ShaderGenValidationReport{};
        storage.last_report_material_name.clear();
        storage.has_last_report = false;
        storage.report_sequence = 0;
        storage.history_reports.clear();
    }

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshotStorage()
    {
        auto &storage = GetShaderGenProfilerStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);
        return storage.snapshot;
    }

    bool GetShaderGenLastValidationReportStorage(ShaderGenValidationReport &out_report,
                                                 std::string *out_material_name)
    {
        auto &storage = GetShaderGenValidationReportStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        if (!storage.has_last_report)
            return false;

        out_report = storage.last_report;

        if (out_material_name)
            *out_material_name = storage.last_report_material_name;

        return true;
    }

    std::vector<ShaderGenValidationReportRecord> GetRecentShaderGenValidationReportsStorage(uint32_t max_count)
    {
        auto &storage = GetShaderGenValidationReportStorage();
        std::lock_guard<std::mutex> lock(storage.mutex);

        std::vector<ShaderGenValidationReportRecord> out;
        if (max_count == 0 || storage.history_reports.empty())
            return out;

        const size_t take = std::min<size_t>(max_count, storage.history_reports.size());
        out.reserve(take);

        auto it = storage.history_reports.rbegin();
        for (size_t i = 0; i < take && it != storage.history_reports.rend(); ++i, ++it)
            out.emplace_back(*it);

        return out;
    }

    std::map<std::string, std::vector<ShaderGenValidationReportRecord>> GetRecentShaderGenValidationReportsByMaterialStorage(uint32_t max_per_material,
                                                                                                                               uint32_t max_total)
    {
        if (max_per_material == 0 || max_total == 0)
            return {};

        const auto recent = GetRecentShaderGenValidationReportsStorage(max_total);
        return BuildRecentValidationReportsByMaterial(recent, max_per_material);
    }

    std::map<std::string, uint32_t> GetRecentShaderGenValidationCategoryHistogramStorage(uint32_t max_count)
    {
        std::map<std::string, uint32_t> histogram;
        if (max_count == 0)
            return histogram;

        const auto recent = GetRecentShaderGenValidationReportsStorage(max_count);
        for (const auto &record : recent)
        {
            const std::string category = record.report.category.empty() ? "Uncategorized" : record.report.category;
            ++histogram[category];
        }

        return histogram;
    }

    std::map<std::string, std::map<std::string, uint32_t>> GetRecentShaderGenValidationMaterialCategoryMatrixStorage(uint32_t max_count)
    {
        std::map<std::string, std::map<std::string, uint32_t>> matrix;
        if (max_count == 0)
            return matrix;

        const auto recent = GetRecentShaderGenValidationReportsStorage(max_count);
        for (const auto &record : recent)
        {
            const std::string material = record.material_name.empty() ? "<unnamed-material>" : record.material_name;
            const std::string category = record.report.category.empty() ? "Uncategorized" : record.report.category;

            ++matrix[material][category];
        }

        return matrix;
    }
}
