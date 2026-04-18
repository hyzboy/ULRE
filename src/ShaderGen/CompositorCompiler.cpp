/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
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

        cfg.camera            = cfg.camera            || infer_has_camera;
        cfg.sky               = cfg.sky               || infer_has_sky;
        cfg.local_to_world    = cfg.local_to_world    || infer_has_l2w;
        cfg.material_instance = cfg.material_instance || infer_has_mi;

        MaterialCreateInfo *mci = new MaterialCreateInfo(&cfg);
        if (profile)
            mci->SetDevice(profile);

        auto FailAfterMci = [&](const char *reason) -> MaterialCreateInfo *
        {
            if (diagnostics)
            {
                *diagnostics = reason ? reason : "<unknown>";
                *diagnostics += " (";
                *diagnostics += BuildShaderDataSchemaDebugText(def);
                *diagnostics += ")";
            }
            delete mci;
            return nullptr;
        };

        uint32_t mi_stage_bits = uint32_t(ShaderStage::Fragment);

        if (def.ubo_descriptors)
        {
            for (const auto semantic : *def.ubo_descriptors)
            {
                if (!mci->AddUBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailAfterMci("AddUBO() failed");
            }
        }

        if (def.ssbo_descriptors)
        {
            for (const auto semantic : *def.ssbo_descriptors)
            {
                if (semantic == SSBODescriptorSemantic::TransformData)
                {
                    mci->SetLocalToWorld(kDefaultDescriptorStageBits);
                    continue;
                }

                if (semantic == SSBODescriptorSemantic::MaterialBindingInstanceData)
                {
                    mi_stage_bits = kDefaultDescriptorStageBits;
                    continue;
                }

                if (!mci->AddSSBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailAfterMci("AddSSBO() failed");
            }
        }

        if (def.texture_samplers)
        {
            for (const auto &[slot, descriptor] : *def.texture_samplers)
            {
                if (!RangeCheck(descriptor.sampler_type))
                    return FailAfterMci("texture sampler slot has invalid SamplerType");

                if (!mci->AddTextureSampler(mci->GetShaderStage(),
                                            descriptor.sampler_type,
                                            slot,
                                            descriptor.channel_hint))
                {
                    return FailAfterMci("AddTextureSampler(slot) failed");
                }
            }
        }

        ShaderCreateInfoVertex *vsc = mci->GetVertexShader();
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
                return FailAfterMci("shader data schema has zero byte size");

            if (!mci->SetMaterialInstance(def.shader_data_schema, schema_info, mi_stage_bits))
                return FailAfterMci("SetMaterialInstance() failed");
        }

        ShaderCreateInfoVertex *vert = mci->GetVertexShader();
        ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

        std::string final_vs_glsl = vs_glsl;
        std::string final_fs_glsl = fs_glsl;

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);
            const std::string schema_include = BuildShaderDataSchemaIncludeText(schema_info);

            if (schema_include.empty())
                return FailAfterMci("shader data schema has no GLSL include path");

            final_vs_glsl = InjectLayoutDefinesPreserveVersion(final_vs_glsl, schema_include);
            final_fs_glsl = InjectLayoutDefinesPreserveVersion(final_fs_glsl, schema_include);
        }

        if (vert)
            vert->SetFinalGLSL(final_vs_glsl);

        if (frag)
            frag->SetFinalGLSL(final_fs_glsl);

        mci->BuildBindingContract();

        if (!InjectLayoutDefines(*mci))
            return FailAfterMci("InjectLayoutDefines() failed");

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

std::string InjectLayoutDefinesPreserveVersion(const std::string &source,const std::string &layout_defs)
{
    if(layout_defs.empty())
        return source;

    if(source.rfind("#version",0)==0)
    {
        const size_t pos=source.find('\n');
        if(pos==std::string::npos)
            return source+"\n"+layout_defs;

        std::string out;
        out.reserve(source.size()+layout_defs.size()+1);
        out.append(source,0,pos+1);
        out.append(layout_defs);
        out.append(source,pos+1,std::string::npos);
        return out;
    }

    return layout_defs+source;
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

    if (!mci->CreateShaderDirect())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: CreateShaderDirect() failed (check GLSLCompiler log) (%s)\n",
            def.name ? def.name : "<unnamed>",
            BuildShaderDataSchemaDebugText(def).c_str());
        delete mci;
        return nullptr;
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
        if (vert) vert->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(vert->GetFinalGLSL(), layout_defs + vert_sampler_defs));
        if (frag) frag->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(frag->GetFinalGLSL(), layout_defs + frag_sampler_defs + frag_mit_defs));
    }

    return true;
}

}  // namespace hgl::graph::mtl
