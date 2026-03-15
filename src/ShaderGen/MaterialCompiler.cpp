/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/mtl/UBOCommon.h>
#include <cstring>
#include <cstdio>
#include <string>

#include "common/MFCommon.h"

namespace hgl::graph::mtl {

static bool CStrEq(const char *lhs, const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

static bool HasVertexEntry(const FixedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        if (CStrEq(def.vertex_entries[i].name, name))
            return true;
    }

    return false;
}

static bool HasDescriptorNamed(const FixedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if (CStrEq(entry.name, name)
         || CStrEq(entry.struct_name, name))
            return true;
    }

    return false;
}

static bool HasPerMaterialDescriptor(const FixedMaterialDef &def)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        if (def.descriptor_entries[i].set_type == DescriptorSetType::Material)
            return true;
    }

    return false;
}

static const ShaderBufferSource *ResolveShaderBufferSourceByStructName(const Material3DCreateConfig *cfg,const char *struct_name)
{
    if(cfg)
    {
        if(const ShaderBufferSource *sbs=cfg->FindPrivateShaderBufferSourceByStructName(struct_name))
            return sbs;
    }

    return FindShaderBufferSourceByStructName(struct_name);
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

    const bool infer_has_camera = HasDescriptorNamed(def, "camera") || HasDescriptorNamed(def, "CameraInfo");
    const bool infer_has_sky    = HasDescriptorNamed(def, "sky")    || HasDescriptorNamed(def, "SkyInfo");
    const bool infer_has_l2w    = HasDescriptorNamed(def, "l2w")    || HasDescriptorNamed(def, "LocalToWorldData");
    const bool infer_has_mi     = HasDescriptorNamed(def, "mtl")
                               || HasDescriptorNamed(def, "MaterialInstanceData")
                               || HasPerMaterialDescriptor(def)
                               || HasVertexEntry(def, Assign::MaterialInstanceID::VIS_NAME)
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
            if (entry.struct_name)
            {
                if (CStrEq(entry.struct_name, SBS_ViewportInfo.struct_name))
                    { mci->AddUBOStruct(stage_bits, SBS_ViewportInfo); break; }
                if (CStrEq(entry.struct_name, SBS_CameraInfo.struct_name))
                    { mci->AddUBOStruct(stage_bits, SBS_CameraInfo); break; }
                if (CStrEq(entry.struct_name, SBS_SkyInfo.struct_name))
                    { mci->AddUBOStruct(stage_bits, SBS_SkyInfo); break; }
                if (CStrEq(entry.struct_name, SBS_LocalToWorld.struct_name))
                    { mci->SetLocalToWorld(stage_bits); break; }
                if (CStrEq(entry.struct_name, SBS_MaterialInstance.struct_name))
                    { mi_stage_bits = stage_bits; break; }

                // Custom UBO via private/global ShaderBufferSource registry
                if (const ShaderBufferSource *sbs = ResolveShaderBufferSourceByStructName(&cfg, entry.struct_name))
                {
                    mci->AddUBOStruct(stage_bits, *sbs);
                    break;
                }
            }
            break;

        case DescriptorKind::SSBO:
            if (entry.struct_name)
            {
                if (CStrEq(entry.struct_name, SBS_LocalToWorld.struct_name))
                    { mci->SetLocalToWorld(stage_bits); break; }
                if (CStrEq(entry.struct_name, SBS_MaterialInstance.struct_name))
                    { mi_stage_bits = stage_bits; break; }

                // Custom SSBO via private/global ShaderBufferSource registry
                if (const ShaderBufferSource *sbs = ResolveShaderBufferSourceByStructName(&cfg, entry.struct_name))
                {
                    mci->AddSSBOStruct(stage_bits, *sbs);
                    break;
                }
            }
            break;

        case DescriptorKind::Texture:
            if (entry.glsl_type)
            {
                TextureType tt;
                const char *glsl_type_str = entry.glsl_type;

                if (CStrEq(glsl_type_str, "sampler2D"))
                    tt = TextureType::Texture2D;
                else if (CStrEq(glsl_type_str, "sampler3D"))
                    tt = TextureType::Texture3D;
                else if (CStrEq(glsl_type_str, "samplerCube"))
                    tt = TextureType::TextureCube;
                else if (CStrEq(glsl_type_str, "sampler2DArray"))
                    tt = TextureType::Texture2DArray;
                else
                    tt = TextureType::Texture2D;

                mci->AddTexture(ShaderStage(stage_bits), entry.set_type, tt, entry.name);
            }
            break;

        case DescriptorKind::TextureSampler:
            if (entry.glsl_type)
            {
                TextureType tt;
                SamplerType st = SamplerType::Sampler2D;
                const char *glsl_type_str = entry.glsl_type;

                if (CStrEq(glsl_type_str, "sampler2D")) {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                } else if (CStrEq(glsl_type_str, "samplerCube")) {
                    tt = TextureType::TextureCube;
                    st = SamplerType::SamplerCube;
                } else if (CStrEq(glsl_type_str, "sampler2DArray")) {
                    tt = TextureType::Texture2DArray;
                    st = SamplerType::Sampler2DArray;
                } else {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                }

                mci->AddTextureSampler(ShaderStage(stage_bits), entry.set_type, st, entry.name);
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
            vsc->AddInput(entry.type, entry.name, entry.input_rate, entry.group);
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