#pragma once

#include <hgl/graph/module/ShaderGenDiffLogDetail.h>
#include <hgl/graph/module/ShaderGenValidationTypes.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;
    }

    class RendererShaderGenAdapter
    {
    public:
        static void ResetProfiler();
        static void ResetValidationReports();
        static ShaderGenProfilerSnapshot GetProfilerSnapshot();
        static bool GetLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name = nullptr);
        static std::vector<ShaderGenValidationReportRecord> GetRecentValidationReports(uint32_t max_count = 64);
        static std::map<std::string, std::vector<ShaderGenValidationReportRecord>> GetRecentValidationReportsByMaterial(uint32_t max_per_material = 4, uint32_t max_total = 128);
        static std::map<std::string, uint32_t> GetRecentValidationReportCategoryHistogram(uint32_t max_count = 128);
        static std::map<std::string, std::map<std::string, uint32_t>> GetRecentValidationMaterialCategoryMatrix(uint32_t max_count = 128);

        ShaderGenValidationReport ValidateMaterialContractReadOnly(const mtl::MaterialCreateInfo &mci,
                                 const mtl::contract::ShaderGenRequest *request,
                                 const mtl::contract::ShaderGenResult *result,
                                 const char *material_name,
                                 ShaderGenDiffLogDetail detail = ShaderGenDiffLogDetail::Full) const;

    private:
        ShaderGenValidationReport ValidateRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        ShaderGenValidationReport ValidateResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        ShaderGenValidationReport ValidatePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, ShaderGenDiffLogDetail detail = ShaderGenDiffLogDetail::Full) const;

    };
}//namespace hgl::graph
