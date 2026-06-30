#include <hgl/shadergen/CompositorCompiler.h>
#include<hgl/log/Logger.h>
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

namespace hgl::graph::mtl
{
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

            GLogError( "%s\n", message.c_str());
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

// Trial/baseline/report entry points were moved to CompositorCompiler_TrialSupport.cpp.

}  // namespace hgl::graph::mtl

