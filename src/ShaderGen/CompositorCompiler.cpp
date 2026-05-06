/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl {

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic);
static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic);
static bool HasPerMaterialDescriptor(const StaticMaterialDef &def);

namespace
{
    static constexpr uint32_t kDefaultDescriptorStageBits = uint32_t(ShaderStage::VertexFragment);

    static bool ResolveConfiguredCameraRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsCamera);

        return cfg.camera;
    }

    static bool ResolveConfiguredSkyRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsSky);

        return cfg.sky;
    }

    static void AppendDiagnosticLine(std::string *diagnostics, const std::string &line)
    {
        if (!diagnostics || line.empty())
            return;

        if (!diagnostics->empty())
            *diagnostics += '\n';

        *diagnostics += line;
    }

    static void EmitInferenceMismatchDiagnostics(
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

    static std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def)
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

    static std::string BuildShaderDataSchemaIncludeText(const ShaderDataSchemaInfo &schema_info)
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

    static MaterialCreateInfo *CreatePreparedCompositorMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const StaticMaterialDef &def,
        const std::string &vs_glsl,
        const std::string &fs_glsl,
        const Material3DCreateConfig *config,
        std::string *diagnostics)
    {
        if (diagnostics)
            diagnostics->clear();

        if (vs_glsl.empty() || fs_glsl.empty())
        {
            if (diagnostics)
                *diagnostics = "vs_glsl or fs_glsl is empty";
            return nullptr;
        }

        Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
        cfg.prim = config ? config->prim : def.primitive_type;
        cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

        const bool infer_has_camera = HasUBOSemantic(def, UBODescriptorSemantic::CameraInfo);
        const bool infer_has_sky    = HasUBOSemantic(def, UBODescriptorSemantic::SkyInfo);
        const bool infer_has_l2w    = HasSSBOSemantic(def, SSBODescriptorSemantic::TransformData);
        const bool infer_has_mi     = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialBindingInstanceData)
                                   || HasPerMaterialDescriptor(def)
                                   || (def.shader_data_schema != ShaderDataSchema::None);

        cfg.local_to_world    = cfg.local_to_world    || infer_has_l2w;
        cfg.material_instance = cfg.material_instance || infer_has_mi;

        EmitInferenceMismatchDiagnostics(def,
                         cfg,
                         infer_has_camera,
                         infer_has_sky,
                         diagnostics);

        MaterialBuilder builder(&cfg);
        if (profile)
            builder.SetDevice(profile);

        auto FailWithBuilder = [&](const char *reason) -> MaterialCreateInfo *
        {
            if (diagnostics)
            {
                *diagnostics = reason ? reason : "<unknown>";
                *diagnostics += " (";
                *diagnostics += BuildShaderDataSchemaDebugText(def);
                *diagnostics += ")";
            }
            return nullptr;
        };

        uint32_t mi_stage_bits = uint32_t(ShaderStage::Fragment);

        if (def.ubo_descriptors)
        {
            for (const auto semantic : *def.ubo_descriptors)
            {
                if (!builder.AddUBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddUBO() failed");
            }
        }

        if (def.ssbo_descriptors)
        {
            for (const auto semantic : *def.ssbo_descriptors)
            {
                if (semantic == SSBODescriptorSemantic::TransformData)
                {
                    builder.SetLocalToWorld(kDefaultDescriptorStageBits);
                    continue;
                }

                if (semantic == SSBODescriptorSemantic::MaterialBindingInstanceData)
                {
                    mi_stage_bits = kDefaultDescriptorStageBits;
                    continue;
                }

                if (!builder.AddSSBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddSSBO() failed");
            }
        }

        if (def.texture_samplers)
        {
            for (const auto &[slot, descriptor] : *def.texture_samplers)
            {
                if (!RangeCheck(descriptor.sampler_type))
                    return FailWithBuilder("texture sampler slot has invalid SamplerType");

                // Use default descriptor stage bits for texture samplers
                if (!builder.AddTextureSampler(kDefaultDescriptorStageBits,
                                            descriptor.sampler_type,
                                            slot,
                                            descriptor.channel_hint))
                {
                    return FailWithBuilder("AddTextureSampler(slot) failed");
                }
            }
        }

        ShaderCreateInfoVertex *vsc = builder.GetVertexShader();
        if (vsc)
        {
            for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
            {
                const FixedVertexEntry &entry = def.vertex_entries[i];
                vsc->AddInput(entry.type, entry.attrib);
            }
        }

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

            if (schema_info.byte_size == 0)
                return FailWithBuilder("shader data schema has zero byte size");

            if (!builder.SetMaterialInstance(def.shader_data_schema, schema_info, mi_stage_bits))
                return FailWithBuilder("SetMaterialInstance() failed");
        }

        ShaderCreateInfoVertex *vert = builder.GetVertexShader();
        ShaderCreateInfo *frag = builder.GetStageShader(ShaderStage::Fragment);

        std::string final_vs_glsl = vs_glsl;
        std::string final_fs_glsl = fs_glsl;

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);
            const std::string schema_include = BuildShaderDataSchemaIncludeText(schema_info);

            if (schema_include.empty())
                return FailWithBuilder("shader data schema has no GLSL include path");

            final_vs_glsl = hgl::graph::internal::InjectAfterVersion(final_vs_glsl, schema_include);
            final_fs_glsl = hgl::graph::internal::InjectAfterVersion(final_fs_glsl, schema_include);
        }

        if (vert)
            vert->SetFinalGLSL(final_vs_glsl);

        if (frag)
            frag->SetFinalGLSL(final_fs_glsl);

        MaterialCreateInfo *mci = builder.BuildSnapshotOnly();
        if (!mci)
            return FailWithBuilder("MaterialBuilder::BuildSnapshotOnly() failed");

        if (!InjectLayoutDefines(*mci))
        {
            delete mci;
            return FailWithBuilder("InjectLayoutDefines() failed");
        }

        return mci;
    }
}

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic)
{
    if (!def.ubo_descriptors || semantic == UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic)
{
    if (!def.ssbo_descriptors || semantic == SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

static bool HasPerMaterialDescriptor(const StaticMaterialDef &def)
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

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    std::string diagnostics;
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(profile,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               config,
                                                               &diagnostics);
    if (!mci)
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.empty() ? "<unknown>" : diagnostics.c_str());
        return nullptr;
    }

    if (!mci->CompileShaderStagesToSPV())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: CompileShaderStagesToSPV() failed (check GLSLCompiler log) (%s)\n",
            def.name ? def.name : "<unnamed>",
            BuildShaderDataSchemaDebugText(def).c_str());
        delete mci;
        return nullptr;
    }

    if (!diagnostics.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s diagnostics: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.c_str());
    }

    return mci;
}

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics)
{
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(nullptr,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               nullptr,
                                                               diagnostics);
    if (!mci)
        return false;

    ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

    out_vs_glsl = vert ? vert->GetFinalGLSL() : std::string();
    out_fs_glsl = frag ? frag->GetFinalGLSL() : std::string();

    delete mci;
    return true;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config)
{
    Material3DCreateConfig cfg3d(
        config ? config->prim : def.primitive_type,
        IncludeCamera::Without,
        config && config->local_to_world ? IncludeL2W::With : IncludeL2W::Without,
        IncludeSky::Without);

    if (config)
    {
        cfg3d.rt_output                         = config->rt_output;
        cfg3d.material_instance                 = config->material_instance;
        cfg3d.shader_stage_flag_bit             = config->shader_stage_flag_bit;
    }

    return CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

bool InjectLayoutDefines(MaterialCreateInfo &mci)
{
    ShaderCreateInfoVertex *vert = mci.GetVertexShader();
    ShaderCreateInfo       *frag = mci.GetStageShader(ShaderStage::Fragment);

    mci.Resort();
    const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(mci);
    const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
    const MaterialDescriptorDB &mdi = mci.GetDescriptorInfo();
    const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Vertex)   : std::string();
    const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Fragment) : std::string();
    const std::string frag_mit_defs     = frag ? hgl::graph::EmitMaterialInstanceTextureGLSL(mdi, ShaderStage::Fragment) : std::string();

    if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty() || !frag_mit_defs.empty())
    {
        if (vert) vert->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(vert->GetFinalGLSL(), layout_defs + vert_sampler_defs));
        if (frag) frag->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(frag->GetFinalGLSL(), layout_defs + frag_sampler_defs + frag_mit_defs));
    }

    return true;
}

}  // namespace hgl::graph::mtl
