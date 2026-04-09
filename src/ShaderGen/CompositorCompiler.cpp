/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
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
        const bool infer_has_mi     = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialInstanceData)
                                   || HasPerMaterialDescriptor(def)
                                   || (def.mi_instance_layout != InstanceDataLayout::None)
                                   || (def.mi_glsl_codes && def.mi_struct_bytes > 0);

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
                *diagnostics = reason ? reason : "<unknown>";
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

                if (semantic == SSBODescriptorSemantic::MaterialInstanceData)
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
            if (def.vertex_attribute_specs && def.vertex_attribute_spec_count > 0)
            {
                for (uint32_t i = 0; i < def.vertex_attribute_spec_count; ++i)
                {
                    if(vsc->AddInput(def.vertex_attribute_specs[i]) <= 0)
                        return FailAfterMci("AddInput(VertexAttributeSpec) failed");
                }
            }
            else if (def.vertex_entries && def.vertex_entry_count > 0)
            {
                for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
                {
                    const FixedVertexEntry &entry = def.vertex_entries[i];
                    if(vsc->AddInput(mtl::MakeLegacyVertexAttributeSpec(entry)) <= 0)
                        return FailAfterMci("AddInput(LegacyFixedVertexEntryBridge) failed");
                }
            }
            else if (def.allow_empty_vertex_declaration)
            {
                // Some procedural materials synthesize positions entirely from gl_VertexIndex.
            }
            else
            {
                return FailAfterMci("No vertex declaration (spec and legacy entries are both empty)");
            }
        }

        if (def.mi_instance_layout != InstanceDataLayout::None)
        {
            mci->SetMaterialInstance(
                GetInstanceDataName(def.mi_instance_layout),
                GetInstanceDataStride(def.mi_instance_layout),
                mi_stage_bits);
        }
        else if (def.mi_glsl_codes && def.mi_struct_bytes > 0)
        {
            mci->SetMaterialInstance(
                def.mi_glsl_codes,
                def.mi_struct_bytes,
                mi_stage_bits);
        }

        ShaderCreateInfoVertex *vert = mci->GetVertexShader();
        ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

        if (vert)
            vert->SetFinalGLSL(vs_glsl);

        if (frag)
            frag->SetFinalGLSL(fs_glsl);

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
            "[CompileCompositorMaterial] material=%s failed: CreateShaderDirect() failed (check GLSLCompiler log)\n",
            def.name ? def.name : "<unnamed>");
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
