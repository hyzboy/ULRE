#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace hgl::graph::mtl
{

std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                       const char *material_name);
bool EnsureDirectoryExists(const char *dir);
bool WriteTextFile(const std::filesystem::path &path,const std::string &text);
std::string SanitizeArtifactName(const char *text);
std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec);
std::string BuildPipelineConfigText(const MaterialCreateConfig &config);
std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report);
std::string BuildSpirvHexText(const ShaderBinary &binary);
bool WriteCompileCompositorPreparedTreeInternal(const MaterialCreateInfo &mci,
                                                const char *material_name,
                                                const char *legacy_root);
bool WriteShaderBuildRouteEvaluationSummary(const ShaderBuildRouteEvaluation &evaluation,const char *filename);

void AppendTrialBatchSummaryFields(std::string &text,
                                  const CompileCompositorTrialBatchReport &report,
                                  const bool markdown_list)
{
    const char *separator = markdown_list ? "- " : ", ";
    const char *line_end = markdown_list ? "\n" : "";
    const char *value_prefix = markdown_list ? "`" : "";
    const char *value_suffix = markdown_list ? "`" : "";

    auto append_one = [&](const char *name, const std::string &value)
    {
        if(!text.empty() && !markdown_list)
            text += separator;
        else if(markdown_list)
            text += separator;

        text += name;
        text += "=";
        text += value_prefix;
        text += value;
        text += value_suffix;
        text += line_end;
    };

    append_one("total_count",std::to_string(report.total_count));
    append_one("legacy_success_count",std::to_string(report.legacy_success_count));
    append_one("legacy_failure_count",std::to_string(report.legacy_failure_count));
    append_one("pipeline_trial_success_count",std::to_string(report.pipeline_trial_success_count));
    append_one("pipeline_trial_failure_count",std::to_string(report.pipeline_trial_failure_count));
    append_one("baseline_report_count",std::to_string(report.baseline_report_count));
    append_one("baseline_compare_success_count",std::to_string(report.baseline_compare_success_count));
    append_one("aggregate_report_written",report.aggregate_report_written ? "true" : "false");
}

std::string BuildTrialAggregateReportText(const std::filesystem::path &trial_root,
                                         const CompileCompositorTrialBatchReport *trial_report)
{
    const std::filesystem::path reports_dir=trial_root/"reports";
    std::string text;
    text += "# ShaderGen 试运行汇总报告（自动生成）\n\n";
    text += "- TrialRoot: `";
    text += trial_root.string();
    text += "`\n\n";

    if(trial_report)
    {
        text += "## Trial Batch Summary\n\n";
        AppendTrialBatchSummaryFields(text,*trial_report,true);
        text += "\n";

        text += "### Direct Compile Failed Materials\n\n";
        if(trial_report->legacy_failed_materials.empty())
            text += "- `<none>`\n\n";
        else
        {
            for(const auto &material_name:trial_report->legacy_failed_materials)
            {
                text += "- `";
                text += material_name;
                text += "`\n";
            }
            text += "\n";
        }

        text += "### Pipeline Trial Failed Materials\n\n";
        if(trial_report->pipeline_trial_failed_materials.empty())
            text += "- `<none>`\n\n";
        else
        {
            for(const auto &material_name:trial_report->pipeline_trial_failed_materials)
            {
                text += "- `";
                text += material_name;
                text += "`\n";
            }
            text += "\n";
        }
    }

    text += "## Per-Material Baseline Reports\n\n";

    bool has_any=false;
    std::error_code ec;
    if(std::filesystem::exists(reports_dir,ec))
    {
        for(const auto &entry:std::filesystem::directory_iterator(reports_dir,ec))
        {
            if(ec)
                break;
            if(!entry.is_regular_file())
                continue;

            const auto filename=entry.path().filename().string();
            if(filename.find("_baseline_compare.md")==std::string::npos)
                continue;

            has_any=true;
            text += "- `";
            text += filename;
            text += "`\n";
        }
    }

    if(!has_any)
        text += "- `<none>`\n";

    return text;
}

std::string BuildBaselineCompareReportText(const CompileCompositorShadowBuildReport &report,
                                           const char *material_name,
                                           const bool direct_compile_success,
                                           const char *direct_compile_summary)
{
    std::string text;
    text.reserve(1024);
    text += "# ShaderGen 基线对比报告（自动生成）\n\n";
    text += "- Material: `";
    text += material_name ? material_name : "<unnamed>";
    text += "`\n";
    text += "- Direct compile: `";
    text += direct_compile_success ? "success" : "failed";
    text += "`\n";
    text += "- Pipeline shadow compile: `";
    text += report.result.success ? "success" : "failed";
    text += "`\n\n";
    text += "## Route-Switch Readiness\n\n";
    text += "- Readiness: `";
    text += report.summary;
    text += "`\n\n";
    text += "## Direct Compile 摘要\n\n";
    text += "- DirectCompileSummary: `";
    text += direct_compile_summary ? direct_compile_summary : (direct_compile_success ? "compile succeeded" : "compile failed");
    text += "`\n\n";
    text += "## Shadow Diagnostics\n\n";

    if(report.result.diagnostics.empty())
        text += "- `<none>`\n";
    else
    {
        for(const auto &diag:report.result.diagnostics)
        {
            text += "- `";
            text += diag.subject;
            text += "`: ";
            text += diag.message;
            text += "\n";
        }
    }

    return text;
}

bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir)
{
    if(!EnsureDirectoryExists(reports_dir))
        return false;

    const std::filesystem::path reports_path(reports_dir);
    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path readiness_path=reports_path/(sanitized_name + "_readiness.txt");
    if(!hgl::graph::WriteShaderBuildRouteEvaluationSummary(report.evaluation,readiness_path.string().c_str()))
        return false;

    const std::filesystem::path diagnostics_path=reports_path/(sanitized_name + "_diagnostics.log");
    return WriteTextFile(diagnostics_path,BuildShadowDiagnosticsText(report,material_name));
}

bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root)
{
    if(!EnsureDirectoryExists(trial_root))
        return false;

    const std::filesystem::path trial_root_path(trial_root);
    const std::filesystem::path reports_path=trial_root_path/"reports";
    if(!EnsureDirectoryExists(reports_path.string().c_str()))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path report_path=reports_path/(sanitized_name + "_baseline_compare.md");
    return WriteTextFile(report_path,
                         BuildBaselineCompareReportText(report,
                                                        material_name,
                                                        legacy_compile_success,
                                                        legacy_summary));
}

bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root)
{
    if(!EnsureDirectoryExists(pipeline_root))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path material_root=std::filesystem::path(pipeline_root)/sanitized_name;
    if(!EnsureDirectoryExists(material_root.string().c_str()))
        return false;

    if(!WriteTextFile(material_root/"descriptor_spec.txt",BuildDescriptorSpecText(report.descriptor_spec)))
        return false;
    if(!WriteTextFile(material_root/"pipeline_config.txt",BuildPipelineConfigText(report.pipeline_config)))
        return false;
    if(!WriteTextFile(material_root/"result_summary.txt",BuildPipelineResultText(report)))
        return false;
    if(!WriteTextFile(material_root/"readiness.txt",report.summary))
        return false;
    if(!WriteTextFile(material_root/"diagnostics.log",BuildShadowDiagnosticsText(report,material_name)))
        return false;

    for(size_t i=0;i<report.result.value.binaries.size();++i)
    {
        const ShaderBinary &binary=report.result.value.binaries[i];
        const std::filesystem::path spv_path=material_root/(std::string("stage_") + std::to_string(i) + ".spv.txt");
        if(!WriteTextFile(spv_path,BuildSpirvHexText(binary)))
            return false;
    }

    return true;
}

bool WriteCompileCompositorLegacyTree(const MaterialCreateInfo &mci,
                                      const char *material_name,
                                      const char *legacy_root)
{
    return WriteCompileCompositorPreparedTreeInternal(mci,material_name,legacy_root);
}

std::string BuildCompileCompositorBaselineCompareCommand(const char *material_name,
                                                         const char *trial_root)
{
    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path trial_root_path(trial_root ? trial_root : "build/shadergen_trial");
    const std::filesystem::path legacy_dir=trial_root_path/"legacy"/sanitized_name;
    const std::filesystem::path pipeline_dir=trial_root_path/"pipeline"/sanitized_name;
    const std::filesystem::path report_path=trial_root_path/"reports"/(sanitized_name + "_baseline_compare.md");
    const std::filesystem::path readiness_path=trial_root_path/"reports"/(sanitized_name + "_readiness.txt");

    std::string command;
    command.reserve(512);
    command += "python shadergen_baseline_compare.py --legacy \"";
    command += legacy_dir.string();
    command += "\" --pipeline \"";
    command += pipeline_dir.string();
    command += "\" --report \"";
    command += report_path.string();
    command += "\" --readiness-file \"";
    command += readiness_path.string();
    command += "\"";
    return command;
}

bool RunCompileCompositorBaselineCompare(const char *material_name,
                                         const char *trial_root)
{
    const std::string command=BuildCompileCompositorBaselineCompareCommand(material_name,trial_root);
    return std::system(command.c_str())==0;
}

bool WriteCompileCompositorTrialAggregateReport(const char *trial_root,
                                                const CompileCompositorTrialBatchReport *report)
{
    const std::filesystem::path trial_root_path(trial_root ? trial_root : "build/shadergen_trial");
    const std::filesystem::path reports_path=trial_root_path/"reports";
    if(!EnsureDirectoryExists(reports_path.string().c_str()))
        return false;

    return WriteTextFile(reports_path/"baseline_compare.md",
                         BuildTrialAggregateReportText(trial_root_path,report));
}

std::string GetCompileCompositorTrialBatchSummary(const CompileCompositorTrialBatchReport &report)
{
    std::string text;
    AppendTrialBatchSummaryFields(text,report,false);

    if(!report.legacy_failed_materials.empty())
    {
        text += ", legacy_failed_materials=";
        for(size_t i=0;i<report.legacy_failed_materials.size();++i)
        {
            if(i>0)
                text += "|";
            text += report.legacy_failed_materials[i];
        }
    }

    if(!report.pipeline_trial_failed_materials.empty())
    {
        text += ", pipeline_trial_failed_materials=";
        for(size_t i=0;i<report.pipeline_trial_failed_materials.size();++i)
        {
            if(i>0)
                text += "|";
            text += report.pipeline_trial_failed_materials[i];
        }
    }

    return text;
}

}  // namespace hgl::graph::mtl
