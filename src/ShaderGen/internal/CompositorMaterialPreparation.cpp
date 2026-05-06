#include <hgl/shadergen/internal/CompositorMaterialPreparation.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <cstdio>
#include <memory>
#include <string>

namespace hgl::graph::mtl::internal {

namespace {

constexpr uint32_t kDefaultDescriptorStageBits = uint32_t(ShaderStage::VertexFragment);
constexpr uint32_t kAllowedDescriptorStageBits =
    uint32_t(ShaderStage::Vertex) |
    uint32_t(ShaderStage::TessControl) |
    uint32_t(ShaderStage::TessEval) |
    uint32_t(ShaderStage::Geometry) |
    uint32_t(ShaderStage::Fragment) |
    uint32_t(ShaderStage::Compute) |
    uint32_t(ShaderStage::ClusterCulling);

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

    return def.texture_samplers && !def.texture_samplers->empty();
}

uint32_t ResolveDescriptorStageBits(const Material3DCreateConfig &cfg)
{
    uint32_t stage_bits = kDefaultDescriptorStageBits;

    if (cfg.shader_stage_flag_bit != 0)
        stage_bits = cfg.shader_stage_flag_bit;

    stage_bits &= kAllowedDescriptorStageBits;

    if ((stage_bits & uint32_t(ShaderStage::Vertex)) == 0)
        stage_bits |= uint32_t(ShaderStage::Vertex);

    if ((stage_bits & uint32_t(ShaderStage::Fragment)) == 0)
        stage_bits |= uint32_t(ShaderStage::Fragment);

    return stage_bits;
}

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

} // namespace

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

std::unique_ptr<MaterialCreateInfo> PrepareCompositorMaterialSnapshotOwned(
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
    cfg.shader_stage_flag_bit = config
        ? ResolveDescriptorStageBits(*config)
        : kDefaultDescriptorStageBits;

    const uint32_t descriptor_stage_bits = ResolveDescriptorStageBits(cfg);

    const bool infer_has_camera = HasUBOSemantic(def, UBODescriptorSemantic::CameraInfo);
    const bool infer_has_sky = HasUBOSemantic(def, UBODescriptorSemantic::SkyInfo);
    const bool infer_has_l2w = HasSSBOSemantic(def, SSBODescriptorSemantic::TransformData);
    const bool infer_has_mi = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialBindingInstanceData)
                           || HasPerMaterialDescriptor(def)
                           || (def.shader_data_schema != ShaderDataSchema::None);

    cfg.local_to_world = cfg.local_to_world || infer_has_l2w;
    cfg.material_instance = cfg.material_instance || infer_has_mi;

    EmitInferenceMismatchDiagnostics(def,
                                     cfg,
                                     infer_has_camera,
                                     infer_has_sky,
                                     diagnostics);

    MaterialBuilder builder(&cfg);
    if (profile)
        builder.SetDevice(profile);

    auto FailWithBuilder = [&](const char *reason) -> std::unique_ptr<MaterialCreateInfo>
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
            if (!builder.AddUBOStruct(descriptor_stage_bits, semantic))
                return FailWithBuilder("AddUBO() failed");
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto semantic : *def.ssbo_descriptors)
        {
            if (semantic == SSBODescriptorSemantic::TransformData)
            {
                builder.SetLocalToWorld(descriptor_stage_bits);
                continue;
            }

            if (semantic == SSBODescriptorSemantic::MaterialBindingInstanceData)
            {
                mi_stage_bits = descriptor_stage_bits;
                continue;
            }

            if (!builder.AddSSBOStruct(descriptor_stage_bits, semantic))
                return FailWithBuilder("AddSSBO() failed");
        }
    }

    if (def.texture_samplers)
    {
        for (const auto &[slot, descriptor] : *def.texture_samplers)
        {
            if (!RangeCheck(descriptor.sampler_type))
                return FailWithBuilder("texture sampler slot has invalid SamplerType");

            if (!builder.AddTextureSampler(descriptor_stage_bits,
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

    std::unique_ptr<MaterialCreateInfo> mci = builder.BuildSnapshotOwned();
    if (!mci)
        return FailWithBuilder("MaterialBuilder::BuildSnapshotOwned() failed");

    if (def.vertex_stream_key)
        mci->AddVertexStreamSSBOs(*def.vertex_stream_key);

    if (!InjectLayoutDefines(*mci))
        return FailWithBuilder("InjectLayoutDefines() failed");

    return mci;
}

MaterialCreateInfo *PrepareCompositorMaterialSnapshot(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material3DCreateConfig *config,
    std::string *diagnostics)
{
    return PrepareCompositorMaterialSnapshotOwned(profile,
                                                  def,
                                                  vs_glsl,
                                                  fs_glsl,
                                                  config,
                                                  diagnostics).release();
}

} // namespace hgl::graph::mtl::internal