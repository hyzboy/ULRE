#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace hgl::graph::mtl
{

bool EnsureDirectoryExists(const char *dir);
bool WriteTextFile(const std::filesystem::path &path,const std::string &text);
std::string SanitizeArtifactName(const char *text);
std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec);
std::string BuildPipelineConfigText(const MaterialCreateConfig &config);
std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report);
std::string BuildSpirvHexText(const ShaderBinary &binary);
bool WriteShaderBuildRouteEvaluationSummary(const ShaderBuildRouteEvaluation &evaluation,const char *filename);

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
    std::string diagnostics_text;
    diagnostics_text.reserve(512);
    diagnostics_text += "material=";
    diagnostics_text += material_name ? material_name : "<unnamed>";
    diagnostics_text += "\n";
    diagnostics_text += "summary=";
    diagnostics_text += report.summary;
    diagnostics_text += "\n";

    for(const auto &diag:report.result.diagnostics)
    {
        diagnostics_text += "diag.subject=";
        diagnostics_text += diag.subject;
        diagnostics_text += ", diag.message=";
        diagnostics_text += diag.message;
        diagnostics_text += "\n";
    }

    return WriteTextFile(diagnostics_path,diagnostics_text);
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

    std::string diagnostics_text;
    diagnostics_text.reserve(512);
    diagnostics_text += "material=";
    diagnostics_text += material_name ? material_name : "<unnamed>";
    diagnostics_text += "\n";
    diagnostics_text += "summary=";
    diagnostics_text += report.summary;
    diagnostics_text += "\n";
    for(const auto &diag:report.result.diagnostics)
    {
        diagnostics_text += "diag.subject=";
        diagnostics_text += diag.subject;
        diagnostics_text += ", diag.message=";
        diagnostics_text += diag.message;
        diagnostics_text += "\n";
    }
    if(!WriteTextFile(material_root/"diagnostics.log",diagnostics_text))
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

}//namespace hgl::graph::mtl
