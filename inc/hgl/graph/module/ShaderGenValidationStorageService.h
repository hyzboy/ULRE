#pragma once

#include <hgl/graph/module/ShaderGenValidationTypes.h>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph
{
    inline constexpr const char *kShaderGenPathDecisionSpvUseMirror = "spv.use_mirror";
    inline constexpr const char *kShaderGenPathDecisionSpvStrictAbort = "spv.strict_abort";
    inline constexpr const char *kShaderGenPathDecisionSpvUseLegacyFallback = "spv.use_legacy_fallback";
    inline constexpr const char *kShaderGenPathDecisionSpvUseLegacyDirect = "spv.use_legacy_direct";

    inline constexpr const char *kShaderGenPathDecisionVertexStrictAbort = "vertex.strict_abort";
    inline constexpr const char *kShaderGenPathDecisionVertexUseLegacy = "vertex.use_legacy";
    inline constexpr const char *kShaderGenPathDecisionVertexUseMirror = "vertex.use_mirror";

    inline constexpr const char *kShaderGenPathDecisionDescriptorStrictAbort = "descriptor.strict_abort";
    inline constexpr const char *kShaderGenPathDecisionDescriptorUseLegacy = "descriptor.use_legacy";
    inline constexpr const char *kShaderGenPathDecisionDescriptorUseMirror = "descriptor.use_mirror";

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
                                       size_t mirror_spv_count);

    void RecordShaderGenContractPathDecision(const char *decision_key);

    void StoreShaderGenValidationReport(const char *material_name,
                                        const ShaderGenValidationReport &report);

    void StoreExternalShaderGenValidationError(const char *material_name,
                                               const char *message,
                                               const char *category = nullptr);

    void ResetShaderGenProfilerStorage();
    void ResetShaderGenValidationReportStorage();

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshotStorage();
    bool GetShaderGenLastValidationReportStorage(ShaderGenValidationReport &out_report,
                                                 std::string *out_material_name = nullptr);

    std::vector<ShaderGenValidationReportRecord> GetRecentShaderGenValidationReportsStorage(uint32_t max_count = 64);
    std::map<std::string, std::vector<ShaderGenValidationReportRecord>> GetRecentShaderGenValidationReportsByMaterialStorage(uint32_t max_per_material = 4,
                                                                                                                               uint32_t max_total = 128);
    std::map<std::string, uint32_t> GetRecentShaderGenValidationCategoryHistogramStorage(uint32_t max_count = 128);
    std::map<std::string, std::map<std::string, uint32_t>> GetRecentShaderGenValidationMaterialCategoryMatrixStorage(uint32_t max_count = 128);
}
