#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include "3d/StandardDescriptorBuilder.h"
#include "3d/Build3DCommon.h"
#include "3d/StandardVariantRouter.h"
#include "3d/MaterialFactory3DCommon.h"
#include "2d/Build2DCommon.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <span>
#include <string>

namespace hgl::graph::mtl {

bool ResolveConfiguredCameraRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsCamera);

        return cfg.camera;
    }

bool ResolveConfiguredSkyRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsSky);

        return cfg.sky;
    }

void AppendDiagnosticLine(std::string *diagnostics, const std::string &line)
    {
        if (!diagnostics || line.empty())
            return;

        if (!diagnostics->empty())
            *diagnostics += '\n';

        *diagnostics += line;
    }

void EmitInferenceMismatchDiagnostics(
        const StaticMaterialDef &def,
        const Material3DCreateConfig &cfg,
        const bool infer_has_camera,
        const bool infer_has_sky,
        std::string *diagnostics)
    {
        const bool configured_camera = ResolveConfiguredCameraRequirement(cfg);
        const bool configured_sky = ResolveConfiguredSkyRequirement(cfg);

        auto emit_one = [&](const char *label, const bool configured, const bool inferred)
        {
            if (configured == inferred)
                return;

            std::string message;
            message.reserve(256);
            message += "[CompositorCompiler] inferred ";
            message += label;
            message += "=";
            message += inferred ? "true" : "false";
            message += " differs from configured/effective=";
            message += configured ? "true" : "false";
            message += " for material='";
            message += def.name ? def.name : "<unnamed>";
            message += "'";

            if (cfg.effective_feature_mask != 0)
            {
                char buf[96]{};
                std::snprintf(buf,
                              sizeof(buf),
                              " effective_feature_mask=0x%016llx",
                              static_cast<unsigned long long>(cfg.effective_feature_mask));
                message += buf;
            }

            message += "; compiler inference is diagnostics-only";

            std::fprintf(stderr, "%s\n", message.c_str());
            AppendDiagnosticLine(diagnostics, message);
        };

        emit_one("camera", configured_camera, infer_has_camera);
        emit_one("sky", configured_sky, infer_has_sky);
    }

std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def)
    {
        if (def.shader_data_schema == ShaderDataSchema::None)
            return std::string("schema=<none>");

        const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

        std::string text;
        text.reserve(128);
        text += "schema=";
        text += std::to_string(static_cast<uint32_t>(def.shader_data_schema));
        text += " file=";
        text += schema_info.glsl_schema_file ? schema_info.glsl_schema_file : "<null>";
        text += " bytes=";
        text += std::to_string(schema_info.byte_size);
        return text;
    }

std::string BuildShaderDataSchemaIncludeText(const ShaderDataSchemaInfo &schema_info)
    {
        if (!schema_info.glsl_schema_file || !schema_info.glsl_schema_file[0])
            return std::string();

        std::string include_text;
        include_text.reserve(48 + std::char_traits<char>::length(schema_info.glsl_schema_file));
        include_text += "#include \"common/schema/";
        include_text += schema_info.glsl_schema_file;
        include_text += "\"\n";
        return include_text;
    }

ShaderBuildDescriptorSpec BuildDescriptorSpecFromStaticMaterialDef(const StaticMaterialDef &def)
    {
        ShaderBuildDescriptorSpec spec{};

        if(def.ubo_descriptors)
        {
            for(const auto semantic:*def.ubo_descriptors)
                spec.ubos.push_back(semantic);
        }

        if(def.ssbo_descriptors)
        {
            for(const auto semantic:*def.ssbo_descriptors)
            {
                if(semantic==SSBODescriptorSemantic::TransformData)
                    continue;

                spec.ssbos.push_back(semantic);
            }
        }

        if(def.shader_data_schema!=ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info=GetShaderDataSchemaInfo(def.shader_data_schema);
            spec.material_instance_schema=def.shader_data_schema;
            spec.material_instance_bytes=schema_info.byte_size;
        }

        return spec;
    }

void AppendShadowBuildDiagnostics(std::string &text,
                                  const ShaderGenResult<ShaderBuildResult> &result)
    {
        for(const auto &diag:result.diagnostics)
        {
            if(!text.empty())
                text += " | ";

            text += diag.subject;
            text += ": ";
            text += diag.message;
        }
    }

std::string SanitizeArtifactName(const char *text)
    {
        if(!text || !*text)
            return std::string("unnamed_material");

        std::string sanitized;
        sanitized.reserve(std::char_traits<char>::length(text));

        for(const char ch:std::string(text))
        {
            if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-')
                sanitized.push_back(ch);
            else
                sanitized.push_back('_');
        }

        return sanitized.empty()?std::string("unnamed_material"):sanitized;
    }

bool EnsureDirectoryExists(const char *dir)
    {
        if(!dir || !*dir)
            return false;

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(dir),ec);
        return !ec;
    }

bool WriteTextFile(const std::filesystem::path &path,const std::string &text)
    {
        std::ofstream ofs(path,std::ios::out|std::ios::trunc);
        if(!ofs.is_open())
            return false;

        ofs << text;
        return ofs.good();
    }

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
            else
            if(markdown_list)
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

std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                       const char *material_name)
    {
        std::string text;
        text.reserve(512);
        text += "material=";
        text += material_name ? material_name : "<unnamed>";
        text += "\n";
        text += "summary=";
        text += report.summary;
        text += "\n";

        for(const auto &diag:report.result.diagnostics)
        {
            text += "diag.subject=";
            text += diag.subject;
            text += ", diag.message=";
            text += diag.message;
            text += "\n";
        }

        return text;
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
            {
                text += "- `<none>`\n\n";
            }
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
            {
                text += "- `<none>`\n\n";
            }
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

std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec)
    {
        std::string text;
        text += "ubos=";
        for(size_t i=0;i<spec.ubos.size();++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(static_cast<int>(spec.ubos[i]));
        }

        text += "\nssbos=";
        for(size_t i=0;i<spec.ssbos.size();++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(static_cast<int>(spec.ssbos[i]));
        }

        text += "\nmaterial_instance_bytes=";
        text += std::to_string(spec.material_instance_bytes);
        text += "\nmaterial_instance_schema=";
        text += std::to_string(static_cast<int>(spec.material_instance_schema));
        text += "\n";
        return text;
    }

const char *GetShaderStageArtifactName(const ShaderStage stage)
    {
        switch(stage)
        {
            case ShaderStage::Vertex:   return "vertex";
            case ShaderStage::Fragment: return "fragment";
            case ShaderStage::Geometry: return "geometry";
            case ShaderStage::Compute:  return "compute";
            default:                    return "unknown";
        }
    }

std::string BuildDescriptorInfoText(const MaterialDescriptorDB &descriptor_db,
                                    const DescriptorBindingSlots &binding_contract)
    {
        std::string text;
        text += "descriptor_count=";
        text += std::to_string(descriptor_db.GetCount());
        text += "\nubos=";

        for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(binding_contract.ubos[i]);
        }

        text += "\nssbos=";

        for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(binding_contract.ssbos[i]);
        }

        text += "\n";
        return text;
    }

std::string BuildMaterialBlocksText(const MaterialCreateInfo &mci)
    {
        const auto &material_instance=mci.GetMaterialInstance();
        const auto &local_to_world=mci.GetLocalToWorld();

        std::string text;
        text += "material_instance.enabled=";
        text += material_instance.IsActive() ? "true" : "false";
        text += "\nmaterial_instance.stage_bits=";
        text += std::to_string(material_instance.stage_bits);
        text += "\nmaterial_instance.stride=";
        text += std::to_string(material_instance.stride);
        text += "\nmaterial_instance.schema=";
        text += std::to_string(static_cast<int>(material_instance.schema));
        text += "\nmaterial_instance.schema_file=";
        text += material_instance.schema_file;
        text += "\nlocal_to_world.enabled=";
        text += local_to_world.enabled ? "true" : "false";
        text += "\nlocal_to_world.stage_bits=";
        text += std::to_string(local_to_world.stage_bits);
        text += "\n";
        return text;
    }

bool WriteLegacyShaderArtifacts(const std::filesystem::path &material_root,
                                const MaterialCreateInfo &mci)
    {
        for(const auto &[stage,shader]:mci.GetShaderMap())
        {
            if(!shader)
                continue;

            const char *stage_name=GetShaderStageArtifactName(stage);
            const std::filesystem::path glsl_path=material_root/(std::string(stage_name) + ".glsl");
            if(!WriteTextFile(glsl_path,shader->GetFinalGLSL()))
                return false;

            const uint32 *spv_data=shader->GetSPVData();
            const size_t spv_size=shader->GetSPVSize();

            if(spv_data&&spv_size>0)
            {
                std::string spv_text;
                const size_t word_count=spv_size/sizeof(uint32_t);
                spv_text.reserve(word_count*9);

                for(size_t i=0;i<word_count;++i)
                {
                    char buf[16]{};
                    std::snprintf(buf,sizeof(buf),"%08X",spv_data[i]);
                    spv_text += buf;
                    spv_text += '\n';
                }

                const std::filesystem::path spv_path=material_root/(std::string(stage_name) + ".spv.txt");
                if(!WriteTextFile(spv_path,spv_text))
                    return false;
            }
        }

        return true;
    }

bool WriteCompileCompositorPreparedTreeInternal(const MaterialCreateInfo &mci,
                                                const char *material_name,
                                                const char *legacy_root)
{
    if(!EnsureDirectoryExists(legacy_root))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path material_root=std::filesystem::path(legacy_root)/sanitized_name;

    if(!EnsureDirectoryExists(material_root.string().c_str()))
        return false;

    if(!WriteTextFile(material_root/"descriptor_info.txt",
                      BuildDescriptorInfoText(mci.GetDescriptorInfo(),mci.GetBindingContract())))
        return false;

    if(!WriteTextFile(material_root/"material_blocks.txt",BuildMaterialBlocksText(mci)))
        return false;

    return WriteLegacyShaderArtifacts(material_root,mci);
}

std::string BuildPipelineConfigText(const MaterialCreateConfig &config)
    {
        std::string text;
        text += "shader_stage_flag_bit=";
        text += std::to_string(config.shader_stage_flag_bit);
        text += "\nmaterial_instance=";
        text += config.material_instance ? "true" : "false";
        text += "\nlocal_to_world=";
        text += config.local_to_world ? "true" : "false";
        text += "\nsampler_feature_bits_override=";
        text += std::to_string(config.sampler_feature_bits_override);
        text += "\ntexture_source_bits_override=";
        text += std::to_string(config.texture_source_bits_override);
        text += "\n";
        return text;
    }

std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report)
    {
        std::string text;
        text += "success=";
        text += report.result.success ? "true" : "false";
        text += "\nfinal_state=";
        text += std::to_string(static_cast<int>(report.result.value.final_state));
        text += "\ndescriptor_count=";
        text += std::to_string(report.result.value.descriptor_count);
        text += "\nlayout_finalized=";
        text += report.result.value.layout_finalized ? "true" : "false";
        text += "\nbinary_count=";
        text += std::to_string(report.result.value.binaries.size());
        text += "\n";
        return text;
    }

std::string BuildSpirvHexText(const ShaderBinary &binary)
    {
        std::string text;
        text.reserve(binary.spirv.size()*9);

        for(size_t i=0;i<binary.spirv.size();++i)
        {
            char buf[16]{};
            std::snprintf(buf,sizeof(buf),"%08X",binary.spirv[i]);
            text += buf;
            text += '\n';
        }

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
        {
            text += "- `<none>`\n";
        }
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

void EmitCompileCompositorShadowTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                               const char *material_name)
{
    std::fprintf(stderr,
        "[CompileCompositorMaterial] material=%s pipeline_shadow: %s\n",
        material_name ? material_name : "<unnamed>",
        shadow_report.summary.empty() ? "<empty>" : shadow_report.summary.c_str());

    if(WriteCompileCompositorShadowBuildArtifacts(shadow_report,material_name))
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts=build/shadergen_trial/reports\n",
            material_name ? material_name : "<unnamed>");
    }
    else
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts_write_failed\n",
            material_name ? material_name : "<unnamed>");
    }

    if(WriteCompileCompositorShadowPipelineTree(shadow_report,material_name))
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow_tree=build/shadergen_trial/pipeline\n",
            material_name ? material_name : "<unnamed>");
    }
    else
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow_tree_write_failed\n",
            material_name ? material_name : "<unnamed>");
    }
}

void EmitCompileCompositorFailureAndTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                                   const bool has_shadow_report,
                                                   const char *material_name,
                                                   const char *failure_text,
                                                   const char *baseline_summary)
{
    if(has_shadow_report)
    {
        WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                  material_name,
                                                  false,
                                                  baseline_summary);
    }

    std::fprintf(stderr,
        "[CompileCompositorMaterial] material=%s failed: %s\n",
        material_name ? material_name : "<unnamed>",
        failure_text ? failure_text : "<unknown>");
}

void EmitCompileCompositorSuccessAndTrialArtifacts(const CompileCompositorShadowBuildReport &shadow_report,
                                                   const bool has_shadow_report,
                                                   const MaterialCreateInfo &mci,
                                                   const char *material_name,
                                                   const char *baseline_summary)
{
    if(has_shadow_report)
    {
        if(WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                     material_name,
                                                     true,
                                                     baseline_summary))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report=build/shadergen_trial/reports\n",
                material_name ? material_name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report_write_failed\n",
                material_name ? material_name : "<unnamed>");
        }

        if(RunCompileCompositorBaselineCompare(material_name))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_script=build/shadergen_trial/reports\n",
                material_name ? material_name : "<unnamed>");

            if(WriteCompileCompositorTrialAggregateReport())
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial] material=%s baseline_compare_aggregate=build/shadergen_trial/reports/baseline_compare.md\n",
                    material_name ? material_name : "<unnamed>");
            }
            else
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial] material=%s baseline_compare_aggregate_write_failed\n",
                    material_name ? material_name : "<unnamed>");
            }
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_script_failed\n",
                material_name ? material_name : "<unnamed>");
        }
    }

    if(WriteCompileCompositorLegacyTree(mci,material_name))
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s legacy_tree=build/shadergen_trial/legacy\n",
            material_name ? material_name : "<unnamed>");
    }
    else
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s legacy_tree_write_failed\n",
            material_name ? material_name : "<unnamed>");
    }
}

void EmitCompileCompositorPrepareFailure(const CompileCompositorShadowBuildReport &shadow_report,
                                         const bool has_shadow_report,
                                         const char *material_name,
                                         const std::string &diagnostics)
{
    const char *failure_text=diagnostics.empty() ? "<unknown>" : diagnostics.c_str();
    const char *baseline_summary=diagnostics.empty() ? "ShaderBuildPipeline.PrepareMaterialCreateInfo failed" : diagnostics.c_str();

    EmitCompileCompositorFailureAndTrialArtifacts(shadow_report,
                                                  has_shadow_report,
                                                  material_name,
                                                  failure_text,
                                                  baseline_summary);
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
    if(!WriteShaderBuildRouteEvaluationSummary(report.evaluation,readiness_path.string().c_str()))
        return false;

    const std::filesystem::path diagnostics_path=reports_path/(sanitized_name + "_diagnostics.log");
    return WriteTextFile(diagnostics_path,BuildShadowDiagnosticsText(report,material_name));
}

bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               const bool legacy_compile_success,
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

bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic)
{
    if (!def.ubo_descriptors || semantic == UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic)
{
    if (!def.ssbo_descriptors || semantic == SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

bool HasPerMaterialDescriptor(const StaticMaterialDef &def)
{
    if (def.ubo_descriptors)
    {
        for (const auto semantic : *def.ubo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto semantic : *def.ssbo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.texture_samplers)
    {
        if (!def.texture_samplers->empty())
            return true;
    }

    return false;
}

}  // namespace hgl::graph::mtl

