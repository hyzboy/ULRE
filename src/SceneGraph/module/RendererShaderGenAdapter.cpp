#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/graph/module/ShaderGenMirrorDiffPresenter.h>
#include <hgl/graph/module/ShaderGenValidationReportUtils.h>
#include <hgl/graph/module/ShaderGenValidationStorageService.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/contract/ShaderGenContractValidator.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <cstdio>

namespace hgl::graph
{
    namespace
    {
        inline const char *NormalizeShaderGenValidationMaterialName(const char *material_name)
        {
            return (material_name && material_name[0]) ? material_name : "<unnamed-material>";
        }

        inline ShaderGenValidationReport BuildMirrorPrebuildFailureValidationReport(const char *material_name, bool request_is_null)
        {
            ShaderGenValidationReport report;

            char msg[256] = {};
            std::snprintf(msg,
                          sizeof(msg),
                          "material=%s failed to build mirror result",
                          material_name);

            AddShaderGenValidationError(report, msg);
            report.diff_valid = false;
            report.result_valid = false;
            report.request_result_valid = request_is_null;
            report.overall_valid = false;

            return report;
        }

        inline void FinalizeAndStoreShaderGenValidationReport(const char *material_name, ShaderGenValidationReport &report)
        {
            RecomputeShaderGenValidationOverallValid(report);
            StoreShaderGenValidationReport(material_name, report);
        }
    }//namespace

    ShaderGenValidationReport RendererShaderGenAdapter::ValidateResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        ShaderGenValidationReport report;
        report.result_valid = true;

        const auto contract_check = mtl::contract::ValidateShaderGenResult(result, material_name);
        ApplyShaderGenContractValidationResult(report, contract_check, report.result_valid);

        return report;
    }

    ShaderGenValidationReport RendererShaderGenAdapter::ValidatePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, ShaderGenDiffLogDetail detail) const
    {
        ShaderGenValidationReport report;
        report.diff_valid = true;
        report.result_valid = true;

        const bool diff_ok = PresentShaderGenMirrorDiff(mci, result, material_name, detail, &report);
        if (!diff_ok)
            report.diff_valid = false;

        const ShaderGenValidationReport result_report = ValidateResultReadOnly(result, material_name);
        report.result_valid = result_report.result_valid;
        MergeShaderGenValidationReport(report, result_report);

        FinalizeAndStoreShaderGenValidationReport(material_name, report);
        return report;
    }

    ShaderGenValidationReport RendererShaderGenAdapter::ValidateRequestResultReadOnly(const mtl::contract::ShaderGenRequest &request, const mtl::contract::ShaderGenResult &result, const char *material_name) const
    {
        ShaderGenValidationReport report;
        report.request_result_valid = true;

        const auto contract_check = mtl::contract::ValidateShaderGenRequestResult(request, result, material_name);
        ApplyShaderGenContractValidationResult(report, contract_check, report.request_result_valid);

        FinalizeAndStoreShaderGenValidationReport(material_name, report);
        return report;
    }

    ShaderGenValidationReport RendererShaderGenAdapter::ValidateMaterialContractReadOnly(const mtl::MaterialCreateInfo &mci,
                                                                                                           const mtl::contract::ShaderGenRequest *request,
                                                                                                           const mtl::contract::ShaderGenResult *result,
                                                                                                           const char *material_name,
                                                                                                           ShaderGenDiffLogDetail detail) const
    {
        ShaderGenValidationReport report;

        const char *mat_name = NormalizeShaderGenValidationMaterialName(material_name);

        mtl::contract::ShaderGenResult built_result;
        const mtl::contract::ShaderGenResult *resolved_result = result;

        if (!resolved_result)
        {
            if (!mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(mci, built_result))
            {
                report = BuildMirrorPrebuildFailureValidationReport(mat_name, request == nullptr);
                StoreShaderGenValidationReport(mat_name, report);
                return report;
            }

            resolved_result = &built_result;
        }

        report = ValidatePairReadOnly(mci, *resolved_result, mat_name, detail);

        if (request)
        {
            const ShaderGenValidationReport req_report = ValidateRequestResultReadOnly(*request, *resolved_result, mat_name);

            report.request_result_valid = req_report.request_result_valid;
            MergeShaderGenValidationReport(report, req_report);
            FinalizeAndStoreShaderGenValidationReport(mat_name, report);
        }

        return report;
    }

    void RendererShaderGenAdapter::ResetProfiler()
    {
        ResetShaderGenProfilerStorage();
    }

    void RendererShaderGenAdapter::ResetValidationReports()
    {
        ResetShaderGenValidationReportStorage();
    }

    ShaderGenProfilerSnapshot RendererShaderGenAdapter::GetProfilerSnapshot()
    {
        return GetShaderGenProfilerSnapshotStorage();
    }

    bool RendererShaderGenAdapter::GetLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name)
    {
        return GetShaderGenLastValidationReportStorage(out_report, out_material_name);
    }

    std::vector<ShaderGenValidationReportRecord> RendererShaderGenAdapter::GetRecentValidationReports(uint32_t max_count)
    {
        return GetRecentShaderGenValidationReportsStorage(max_count);
    }

    std::map<std::string, std::vector<ShaderGenValidationReportRecord>> RendererShaderGenAdapter::GetRecentValidationReportsByMaterial(uint32_t max_per_material, uint32_t max_total)
    {
        return GetRecentShaderGenValidationReportsByMaterialStorage(max_per_material, max_total);
    }

    std::map<std::string, uint32_t> RendererShaderGenAdapter::GetRecentValidationReportCategoryHistogram(uint32_t max_count)
    {
        return GetRecentShaderGenValidationCategoryHistogramStorage(max_count);
    }

    std::map<std::string, std::map<std::string, uint32_t>> RendererShaderGenAdapter::GetRecentValidationMaterialCategoryMatrix(uint32_t max_count)
    {
        return GetRecentShaderGenValidationMaterialCategoryMatrixStorage(max_count);
    }

}//namespace hgl::graph
