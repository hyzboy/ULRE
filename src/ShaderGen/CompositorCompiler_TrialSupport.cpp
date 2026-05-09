#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace hgl::graph::mtl
{

bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir)
{
    if(!EnsureDirectoryExists(reports_dir))
        return false;

    const std::filesystem::path reports_path(reports_dir);
    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path readiness_path=reports_path/(sanitized_name + "_readiness.txt");
    if(!WriteShaderBuildRouteEvaluationSummary(report.evaluation,readiness_path.string().c_str()))
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
