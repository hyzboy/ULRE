#include <hgl/graph/module/ShaderGenValidationQueryBridge.h>
#include <hgl/graph/module/ShaderGenValidationStorageService.h>

namespace hgl::graph
{
    void ResetShaderGenProfilerFallback()
    {
        ResetShaderGenProfilerStorage();
    }

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshotFallback()
    {
        return GetShaderGenProfilerSnapshotStorage();
    }

    bool GetShaderGenLastValidationReportFallback(ShaderGenValidationReport &out_report,
                                                  std::string *out_material_name)
    {
        return GetShaderGenLastValidationReportStorage(out_report, out_material_name);
    }

    std::vector<ShaderGenValidationReportRecord> GetShaderGenRecentValidationReportsFallback(uint32_t max_count)
    {
        return GetRecentShaderGenValidationReportsStorage(max_count);
    }

    std::map<std::string, std::vector<ShaderGenValidationReportRecord>> GetShaderGenRecentValidationReportsByMaterialFallback(uint32_t max_per_material,
                                                                                                                               uint32_t max_total)
    {
        return GetRecentShaderGenValidationReportsByMaterialStorage(max_per_material, max_total);
    }

    std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogramFallback(uint32_t max_count)
    {
        return GetRecentShaderGenValidationCategoryHistogramStorage(max_count);
    }
}
