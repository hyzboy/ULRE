#pragma once

#include <hgl/graph/module/ShaderGenValidationTypes.h>
#include <map>
#include <string>
#include <vector>

namespace hgl::graph
{
    void ResetShaderGenProfilerFallback();

    ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshotFallback();

    bool GetShaderGenLastValidationReportFallback(ShaderGenValidationReport &out_report,
                                                  std::string *out_material_name = nullptr);

    std::vector<ShaderGenValidationReportRecord> GetShaderGenRecentValidationReportsFallback(uint32_t max_count = 64);

    std::map<std::string, std::vector<ShaderGenValidationReportRecord>> GetShaderGenRecentValidationReportsByMaterialFallback(uint32_t max_per_material = 4,
                                                                                                                               uint32_t max_total = 128);

    std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogramFallback(uint32_t max_count = 128);
}
