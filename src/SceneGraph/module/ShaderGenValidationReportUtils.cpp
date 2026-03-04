#include <hgl/graph/module/ShaderGenValidationReportUtils.h>
#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/shadergen/contract/ShaderGenContractValidator.h>

namespace hgl::graph
{
    void AddShaderGenValidationWarning(ShaderGenValidationReport &report,
                                       const std::string &message)
    {
        report.warnings.emplace_back(message);
        ++report.warning_count;
    }

    void AddShaderGenValidationError(ShaderGenValidationReport &report,
                                     const std::string &message)
    {
        report.errors.emplace_back(message);
        ++report.error_count;
        report.overall_valid = false;
    }

    void MergeShaderGenValidationReport(ShaderGenValidationReport &dst,
                                        const ShaderGenValidationReport &src)
    {
        dst.overall_valid = dst.overall_valid && src.overall_valid;
        dst.warning_count += src.warning_count;
        dst.error_count += src.error_count;

        if (dst.category.empty() && !src.category.empty())
            dst.category = src.category;

        dst.warnings.insert(dst.warnings.end(), src.warnings.begin(), src.warnings.end());
        dst.errors.insert(dst.errors.end(), src.errors.begin(), src.errors.end());
    }

    void RecomputeShaderGenValidationOverallValid(ShaderGenValidationReport &report)
    {
        report.overall_valid = report.diff_valid
                            && report.result_valid
                            && report.request_result_valid
                            && report.error_count == 0;
    }

    void ApplyShaderGenContractValidationResult(ShaderGenValidationReport &report,
                                                const mtl::contract::ShaderGenContractValidationResult &contract_check,
                                                bool &valid_field)
    {
        valid_field = contract_check.valid;

        report.warning_count += contract_check.warning_count;
        report.error_count += contract_check.error_count;
        report.warnings.insert(report.warnings.end(), contract_check.warnings.begin(), contract_check.warnings.end());
        report.errors.insert(report.errors.end(), contract_check.errors.begin(), contract_check.errors.end());

        if (!contract_check.valid)
        {
            if (report.category.empty())
            {
                for (const auto &err : contract_check.errors)
                {
                    if (err.find("profile limit exceeded") != std::string::npos ||
                        err.find("profile feature mismatch") != std::string::npos)
                    {
                        report.category = kShaderGenStrictGateProfileCategory;
                        break;
                    }
                }
            }

            report.overall_valid = false;
        }
    }
}
