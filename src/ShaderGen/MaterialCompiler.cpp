/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 FixedMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorInfo
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutBuilder.h>
#include <hgl/shadergen/ShaderLayoutDefineEmitter.h>
#include <hgl/shadergen/SimpleSamplerGLSLEmitter.h>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl {

static bool HasUBOSemantic(const FixedMaterialDef &def, const UBODescriptorSemantic semantic)
{
    if (!def.ubo_descriptors || semantic == UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

static bool HasSSBOSemantic(const FixedMaterialDef &def, const SSBODescriptorSemantic semantic)
{
    if (!def.ssbo_descriptors || semantic == SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

static bool HasPerMaterialDescriptor(const FixedMaterialDef &def)
{
    if (def.ubo_descriptors)
    {
        for (const auto &[semantic, stage_flags] : *def.ubo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;

            (void)stage_flags;
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto &[semantic, stage_flags] : *def.ssbo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;

            (void)stage_flags;
        }
    }

    if (def.texture_samplers)
    {
        for (const auto &[slot, descriptor] : *def.texture_samplers)
        {
            if (descriptor.set_type == SET_TYPE_MATERIAL)
                return true;

            (void)slot;
        }
    }

    return false;
}

static std::string InjectLayoutDefinesPreserveVersion(const std::string &source,const std::string &layout_defs)
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
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    if (vs_glsl.empty() || fs_glsl.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s: vs_glsl or fs_glsl is empty\n",
            def.name ? def.name : "<unnamed>");
        return nullptr;
    }

    Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
    cfg.prim = config ? config->prim : def.primitive_type;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    const bool infer_has_camera = HasUBOSemantic(def, UBODescriptorSemantic::CameraInfo);
    const bool infer_has_sky    = HasUBOSemantic(def, UBODescriptorSemantic::SkyInfo);
    const bool infer_has_l2w    = HasSSBOSemantic(def, SSBODescriptorSemantic::LocalToWorld);
    const bool infer_has_mi     = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialInstance)
                               || HasPerMaterialDescriptor(def)
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
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            reason ? reason : "<unknown>");
        delete mci;
        return nullptr;
    };

    uint32_t mi_stage_bits = uint32_t(ShaderStage::Fragment);

    if (def.ubo_descriptors)
    {
        for (const auto &[semantic, stage_bits] : *def.ubo_descriptors)
        {
            if (!mci->AddUBO(stage_bits, semantic))
                return FailAfterMci("AddUBO() failed");
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto &[semantic, stage_bits] : *def.ssbo_descriptors)
        {
            if (semantic == SSBODescriptorSemantic::LocalToWorld)
            {
                mci->SetLocalToWorld(stage_bits);
                continue;
            }

            if (semantic == SSBODescriptorSemantic::MaterialInstance)
            {
                mi_stage_bits = stage_bits;
                continue;
            }

            if (!mci->AddSSBO(stage_bits, semantic))
                return FailAfterMci("AddSSBO() failed");
        }
    }

    if (def.texture_samplers)
    {
        for (const auto &[slot, descriptor] : *def.texture_samplers)
        {
            if (!RangeCheck(descriptor.sampler_type))
                return FailAfterMci("texture sampler slot has invalid SamplerType");

            if (descriptor.set_type != SET_TYPE_MATERIAL)
                return FailAfterMci("texture sampler slot set_type must be SET_TYPE_MATERIAL");

            if (!mci->AddTextureSampler(ShaderStage(descriptor.stage_flags), descriptor.sampler_type, slot, descriptor.channel_hint))
                return FailAfterMci("AddTextureSampler(slot) failed");
        }
    }

    ShaderCreateInfoVertex *vsc = mci->GetVertexShader();
    if (vsc)
    {
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = def.vertex_entries[i];
            vsc->AddInput(entry.type, entry.attrib, entry.input_rate);
        }
    }

    if (def.mi_glsl_codes && def.mi_struct_bytes > 0)
    {
        mci->SetMaterialInstance(
            def.mi_glsl_codes,
            def.mi_struct_bytes,
            mi_stage_bits);
    }

    ShaderCreateInfoVertex   *vert = mci->GetVertexShader();
    ShaderCreateInfo         *frag = mci->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_glsl);

    if (frag)
        frag->SetFinalGLSL(fs_glsl);

    {
        BindingContract contract;
        if (def.ubo_descriptors)
            contract.ubos = *def.ubo_descriptors;
        if (def.ssbo_descriptors)
            contract.ssbos = *def.ssbo_descriptors;
        mci->SetBindingContract(contract);
    }

    {
        mci->Resort();
        const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(*mci);
        const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
        const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(*vert) : std::string();
        const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(*frag) : std::string();
        const std::string frag_mit_defs     = frag ? hgl::graph::EmitMaterialInstanceTextureGLSL(*frag) : std::string();
        if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty() || !frag_mit_defs.empty())
        {
            if (vert) vert->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(vert->GetFinalGLSL(),layout_defs + vert_sampler_defs));
            if (frag) frag->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(frag->GetFinalGLSL(),layout_defs + frag_sampler_defs + frag_mit_defs));
        }
    }

    if (!mci->CreateShaderDirect())
        return FailAfterMci("CreateShaderDirect() failed (check GLSLCompiler log)");

    return mci;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config)
{
    Material3DCreateConfig cfg3d(
        config ? config->prim : def.primitive_type,
        WithCamera::Without,
        config && config->local_to_world ? WithLocalToWorld::With : WithLocalToWorld::Without,
        WithSky::Without);

    if (config)
    {
        cfg3d.rt_output                         = config->rt_output;
        cfg3d.material_instance                 = config->material_instance;
        cfg3d.shader_stage_flag_bit             = config->shader_stage_flag_bit;
    }

    return CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

}  // namespace hgl::graph::mtl
