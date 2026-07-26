/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

static bool CStrEq(const char *lhs, const char *rhs)
{
    // Guard against corrupted/non-string pointers from malformed descriptor entries.
    if (lhs && reinterpret_cast<uintptr_t>(lhs) < 0x10000u)
        return false;
    if (rhs && reinterpret_cast<uintptr_t>(rhs) < 0x10000u)
        return false;
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

static bool HasDescriptorSemantic(const FixedMaterialDef &def, const DescriptorSemantic semantic)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if (entry.semantic == semantic)
            return true;
    }

    return false;
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
            switch (entry.semantic)
            {
            case DescriptorSemantic::ViewportInfo:
                mci->AddUBOStruct(stage_bits, SBS_ViewportInfo);
                break;
            case DescriptorSemantic::CameraInfo:
                mci->AddUBOStruct(stage_bits, SBS_CameraInfo);
                break;
            case DescriptorSemantic::SkyInfo:
                mci->AddUBOStruct(stage_bits, SBS_SkyInfo);
                break;
            case DescriptorSemantic::LocalToWorld:
                mci->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::MaterialInstance:
                mi_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialColorPalette:
                mci->AddUBOStruct(stage_bits, SBS_ColorPattle);
                break;
            default:
                break;
            }
            break;

        case DescriptorKind::SSBO:
            switch (entry.semantic)
            {
            case DescriptorSemantic::LocalToWorld:
                mci->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::LocalToWorldIndexTable:
                mci->AddSSBOStruct(stage_bits, SBS_LocalToWorldIndexRows);
                break;
            case DescriptorSemantic::MaterialInstance:
                mi_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialTextureLayerTable:
                mci->AddSSBOStruct(stage_bits, SBS_MaterialTextureLayerRows);
                break;
            case DescriptorSemantic::MaterialDataIndexTable:
                mci->AddSSBOStruct(stage_bits, SBS_MaterialDataIndexRows);
                break;
            default:
                break;
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
            vsc->AddInput(entry.format, entry.semantic);
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
    //
    // TEXTURE_SLOT_RANGE_SIZE is injected to keep C++ TextureSlot::RANGE_SIZE
    // aligned with the GLSL side.
    //
    // Descriptor numbering is now fixed by descriptor insertion:
    //   MaterialProgram set: mtl=0, mtl_data_index_rows=1, mtl_texture_layer_rows=2
    // TEX_BINDING is generated by 2D builders where needed.
    // Keep only TEXTURE_SLOT_RANGE_SIZE injection here.
    // ─────────────────────────────────────────────────────────────

    constexpr uint32_t texture_slot_range_size = static_cast<uint32_t>(TextureSlot::RANGE_SIZE);

    std::string binding_preamble;
    binding_preamble +=
        "#define TEXTURE_SLOT_RANGE_SIZE " + std::to_string(texture_slot_range_size) + "u\n";


    // GLSL requires #version to be the very first token.
    // Insert the binding defines after the first line (the #version line).
    auto InsertAfterVersionLine = [](const std::string &glsl, const std::string &inject) -> std::string
    {
        if (inject.empty())
            return glsl;
        const auto pos = glsl.find('\n');
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos + 1) + inject + glsl.substr(pos + 1);
    };

    std::string vs_final = InsertAfterVersionLine(vs_glsl, binding_preamble);
    std::string fs_final = InsertAfterVersionLine(fs_glsl, binding_preamble);

    ShaderCreateInfoVertex   *vert = mci->GetVertexShader();
    ShaderCreateInfo         *frag = mci->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);

    // ─────────────────────────────────────────────────────────────
    // Step 6b: Build MaterialResourceLayout from descriptor entries
    // ─────────────────────────────────────────────────────────────

    const MaterialResourceLayout material_resource_layout = BuildMaterialResourceLayout(def.descriptor_entries, def.descriptor_entry_count);
    std::vector<std::string> contract_diagnostics;
    if (!ValidateMaterialResourceLayout(material_resource_layout, contract_diagnostics))
    {
        for (const auto &diag : contract_diagnostics)
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial][MaterialResourceLayout] material=%s: %s\n",
                def.name ? def.name : "<unnamed>",
                diag.c_str());
        }
        return FailAfterMci("MaterialResourceLayout validation failed");
    }

    mci->SetBindingContract(material_resource_layout);

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
