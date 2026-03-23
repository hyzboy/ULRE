/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/DescriptorBindingContract.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutBuilder.h>
#include <hgl/shadergen/ShaderLayoutDefineEmitter.h>
#include <hgl/shadergen/SimpleSamplerGLSLEmitter.h>
#include <cstdio>
#include <string>

namespace hgl::graph::mtl {

static bool HasDescriptorSemantic(const FixedMaterialDef &def, const DescriptorSemantic semantic)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        if (def.descriptor_entries[i].semantic == semantic)
            return true;
    }

    return false;
}

static bool HasPerMaterialDescriptor(const FixedMaterialDef &def)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        if (def.descriptor_entries[i].set_type == SET_TYPE_MATERIAL)
            return true;
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

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial — Compositor 模板完整 GLSL → MaterialCreateInfo
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

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

    // ─────────────────────────────────────────────────────────────
    // Step 1: Config
    // ─────────────────────────────────────────────────────────────

    Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
    cfg.prim = config ? config->prim : def.primitive_type;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    const bool infer_has_camera = HasDescriptorSemantic(def, DescriptorSemantic::CameraInfo);
    const bool infer_has_sky    = HasDescriptorSemantic(def, DescriptorSemantic::SkyInfo);
    const bool infer_has_l2w    = HasDescriptorSemantic(def, DescriptorSemantic::LocalToWorld);
    const bool infer_has_mi     = HasDescriptorSemantic(def, DescriptorSemantic::MaterialInstance)
                               || HasPerMaterialDescriptor(def)
                               || (def.mi_glsl_codes && def.mi_struct_bytes > 0);

    cfg.camera           = cfg.camera           || infer_has_camera;
    cfg.sky              = cfg.sky              || infer_has_sky;
    cfg.local_to_world   = cfg.local_to_world   || infer_has_l2w;
    cfg.material_instance = cfg.material_instance || infer_has_mi;

    // ─────────────────────────────────────────────────────────────
    // Step 2: Create MaterialCreateInfo
    // ─────────────────────────────────────────────────────────────

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

    // ─────────────────────────────────────────────────────────────
    // Step 3: Add Descriptors from FixedDescriptorEntry[]
    // ─────────────────────────────────────────────────────────────

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const FixedDescriptorEntry &entry = def.descriptor_entries[i];
        const uint32_t stage_bits = entry.stage_flags;

        switch (entry.kind)
        {
        case DescriptorKind::UBO:
        {
            if (entry.semantic == DescriptorSemantic::LocalToWorld)
            {
                mci->SetLocalToWorld(stage_bits);
                break;
            }

            if (IsBuiltinDescriptorSemantic(entry.semantic))
            {
                if (mci->AddUBO(stage_bits, entry.semantic))
                    break;
            }

            break;
        }

        case DescriptorKind::SSBO:
        {
            if (entry.semantic == DescriptorSemantic::LocalToWorld)
            {
                mci->SetLocalToWorld(stage_bits);
                break;
            }

            if (entry.semantic == DescriptorSemantic::MaterialInstance)
            {
                mi_stage_bits = stage_bits;
                break;
            }

            if (IsBuiltinDescriptorSemantic(entry.semantic))
            {
                if (mci->AddSSBO(stage_bits, entry.semantic))
                    break;
            }

            break;
        }

        case DescriptorKind::Texture:
            if(!RangeCheck(entry.texture_type))
            {
                return nullptr;
            }

            {
                SamplerSlot slot = SamplerSlot::BaseColor;
                if(!TryGetSlotFromDescriptorName(entry.name,slot))
                    return nullptr;

                mci->AddTexture(ShaderStage(stage_bits), entry.set_type, entry.texture_type, slot);
            }
            break;

        case DescriptorKind::TextureSampler:
            if(!RangeCheck(entry.sampler_type))
            {
                return nullptr;
            }

            {
                SamplerSlot slot = SamplerSlot::BaseColor;
                if(!TryGetSlotFromDescriptorName(entry.name,slot))
                    return nullptr;

                mci->AddTextureSampler(ShaderStage(stage_bits), entry.set_type, entry.sampler_type, slot);
            }
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 4: Add Vertex Inputs from FixedVertexEntry[]
    // ─────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex *vsc = mci->GetVertexShader();
    if (vsc)
    {
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = def.vertex_entries[i];
            vsc->AddInput(entry.type, entry.attrib, entry.input_rate);
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 5: MaterialInstance
    // ─────────────────────────────────────────────────────────────

    if (def.mi_glsl_codes && def.mi_struct_bytes > 0)
    {
        mci->SetMaterialInstance(
            def.mi_glsl_codes,
            def.mi_struct_bytes,
            mi_stage_bits);
    }

    // ─────────────────────────────────────────────────────────────
    // Step 6: Set complete GLSL (bypass ProcXXX pipeline)
    // ─────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex   *vert = mci->GetVertexShader();
    ShaderCreateInfo         *frag = mci->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_glsl);

    if (frag)
        frag->SetFinalGLSL(fs_glsl);

    // ─────────────────────────────────────────────────────────────
    // Step 6b: Build BindingContract from descriptor entries
    // ─────────────────────────────────────────────────────────────

    mci->SetBindingContract(BuildBindingContract(def.descriptor_entries, def.descriptor_entry_count));

    // ─────────────────────────────────────────────────────────────
    // Step 6c: Inject auto-generated layout #defines into GLSL
    // Resort() finalises set/binding numbers; BuildShaderLayoutContract
    // reads them and EmitShaderLayoutDefines produces a #define block.
    // Keep #version as the first directive by inserting the block
    // after #version when present.
    // ─────────────────────────────────────────────────────────────
    {
        mci->Resort();
        const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(*mci);
        const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
        const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(*vert) : std::string();
        const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(*frag) : std::string();
        if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty())
        {
            if (vert) vert->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(vert->GetFinalGLSL(),layout_defs + vert_sampler_defs));
            if (frag) frag->SetFinalGLSL(InjectLayoutDefinesPreserveVersion(frag->GetFinalGLSL(),layout_defs + frag_sampler_defs));
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 7: Compile directly → SPV
    // ─────────────────────────────────────────────────────────────

    if (!mci->CreateShaderDirect())
    {
        return FailAfterMci("CreateShaderDirect() failed (check GLSLCompiler log)");
    }

    return mci;
}

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial — 2D 材质重载
// ═══════════════════════════════════════════════════════════════════════════

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
        cfg3d.private_shader_buffer_sources     = config->private_shader_buffer_sources;
        cfg3d.private_shader_buffer_source_count= config->private_shader_buffer_source_count;
    }

    return CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

}  // namespace hgl::graph::mtl
