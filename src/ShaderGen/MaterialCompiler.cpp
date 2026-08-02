/// MaterialCompiler.cpp — FixedMaterialDef → ShaderProgramBuildSpec 编译器实现
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialInstanceLayout.h>
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

#if defined(_DEBUG)
struct DescriptorShapeKey
{
    DescriptorSetType set_type;
    DescriptorKind kind;
    uint32_t stage_flags;
    DescriptorSemantic semantic;
    TextureSlot texture_slot;
};

static bool ValidateDescriptorShapeExact(
    const char *material_name,
    const FixedDescriptorEntry *entries,
    const uint32_t count,
    const DescriptorShapeKey *expected,
    const uint32_t expected_count,
    std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    if (count != expected_count)
    {
        std::string msg = "E0 shape mismatch: descriptor count changed, material=";
        msg += material_name ? material_name : "<unnamed>";
        msg += ", expected=" + std::to_string(expected_count);
        msg += ", actual=" + std::to_string(count);
        diagnostics.push_back(std::move(msg));
        return false;
    }

    for (uint32_t i = 0; i < expected_count; ++i)
    {
        const auto &a = entries[i];
        const auto &e = expected[i];
        if (a.set_type == e.set_type
         && a.kind == e.kind
         && a.stage_flags == e.stage_flags
         && a.semantic == e.semantic
         && a.texture_slot == e.texture_slot)
        {
            continue;
        }

        std::string msg = "E0 shape mismatch: entry[" + std::to_string(i) + "] changed, material=";
        msg += material_name ? material_name : "<unnamed>";
        diagnostics.push_back(std::move(msg));
        return false;
    }

    return true;
}

static bool ValidateE0DescriptorShapeBaseline(
    const FixedMaterialDef &def,
    const CompositorMaterialBuildConfig &config,
    std::vector<std::string> &diagnostics)
{
    const char *name = def.name ? def.name : "";

    if (std::strcmp(name, "PureColor2D") == 0)
    {
        const DescriptorShapeKey expected[] = {
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::LocalToWorld,           TextureSlot::BaseColor },
            { DescriptorSetType::Transform, DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialSSBOSlotData,    TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialSSBOIndexTable,  TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialTextureLayerTable,TextureSlot::BaseColor },
        };
        return ValidateDescriptorShapeExact(name, def.descriptor_entries, def.descriptor_entry_count, expected, uint32_t(sizeof(expected) / sizeof(expected[0])), diagnostics);
    }

    if (std::strcmp(name, "RectTexture2D") == 0)
    {
        const DescriptorShapeKey expected[] = {
            { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::LocalToWorld,           TextureSlot::BaseColor },
            { DescriptorSetType::Transform, DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor },
        };
        return ValidateDescriptorShapeExact(name, def.descriptor_entries, def.descriptor_entry_count, expected, uint32_t(sizeof(expected) / sizeof(expected[0])), diagnostics);
    }

    if (std::strcmp(name, "Text2D") == 0)
    {
        const DescriptorShapeKey expected[] = {
            { DescriptorSetType::Scene,     DescriptorKind::UBO,     uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::ViewportInfo,          TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialSSBOSlotData,    TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialSSBOIndexTable,  TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::SSBO,    uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), DescriptorSemantic::MaterialTextureLayerTable,TextureSlot::BaseColor },
            { DescriptorSetType::Material,  DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor },
        };
        return ValidateDescriptorShapeExact(name, def.descriptor_entries, def.descriptor_entry_count, expected, uint32_t(sizeof(expected) / sizeof(expected[0])), diagnostics);
    }

    if (std::strcmp(name, "Standard_v1") == 0 || std::strcmp(name, "PBRColor3D") == 0)
    {
        const bool with_cubemap = GetSkyLightDataRequirement(config.sky_ambient_model).need_sky_cubemap;
        const uint32_t base_count = (std::strcmp(name, "Standard_v1") == 0) ? 11u : 8u;
        const uint32_t expect_count = with_cubemap ? (base_count + 1u) : base_count;
        if (def.descriptor_entry_count != expect_count)
        {
            diagnostics.clear();
            std::string msg = "E0 shape mismatch: descriptor count changed, material=";
            msg += name;
            msg += ", expected=" + std::to_string(expect_count);
            msg += ", actual=" + std::to_string(def.descriptor_entry_count);
            diagnostics.push_back(std::move(msg));
            return false;
        }
        return true;
    }

    return true;
}
#endif

static bool IsTextureSlotDeclared(const MaterialDefinition &definition, const TextureSlot slot) noexcept
{
    for (const auto &decl : definition.texture_slot_decls)
    {
        if (decl.slot == slot)
            return true;
    }

    return false;
}

static bool HasLayoutSemantic(const MaterialResourceLayout &layout, const DescriptorSemantic semantic) noexcept
{
    for (const auto &req : layout.requirements)
    {
        if (req.semantic == semantic)
            return true;
    }

    return false;
}

static bool AddMaterialSSBOSlotDescriptor(ShaderProgramBuildSpec &mci,
                                          const MaterialSSBOSlotDecl &decl,
                                          const uint32_t ssbo_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialInstanceLayout(decl.ssbo_type, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!mci.AddStruct(struct_name, glsl_codes))
        return false;

    return mci.AddSSBO(stage_bits, DescriptorSetType::Material, struct_name, decl.name);

    return false;
}

static bool ValidateDefinitionCapabilitySubset(
    const MaterialDefinition &definition,
    const MaterialResourceLayout &layout,
    std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    for (const auto &req : layout.requirements)
    {
        bool allowed = false;

        switch (req.semantic)
        {
        case DescriptorSemantic::ViewportInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::ViewportInfo);
            break;
        case DescriptorSemantic::CameraInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::CameraInfo);
            break;
        case DescriptorSemantic::SkyInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::SkyInfo);
            break;
        case DescriptorSemantic::MaterialColorPalette:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::MaterialColorPalette);
            break;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::LocalToWorldIndexTable:
            allowed = definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
                   && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough;
            break;

        case DescriptorSemantic::MaterialSSBOSlotData:
            allowed = req.ssbo_slot < definition.ssbo_slot_decls.size();
            if (allowed)
                allowed = definition.ssbo_slot_decls[req.ssbo_slot].ssbo_type == req.ssbo_type;
            break;

        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialSSBOIndexTable:
            allowed = !definition.ssbo_slot_decls.empty();
            break;

        case DescriptorSemantic::MaterialTexture:
        case DescriptorSemantic::MaterialSampler:
            allowed = IsTextureSlotDeclared(definition, req.texture_slot);
            break;

        // Sky cubemap sampler is injected by sky-light model and is considered
        // allowed when material declares SkyInfo capability.
        case DescriptorSemantic::SkyCubemapSampler:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::SkyInfo);
            break;

        case DescriptorSemantic::Unknown:
        case DescriptorSemantic::Custom:
            allowed = false;
            break;
        }

        if (allowed)
            continue;

        std::string message = "Definition capability subset violation: semantic=";
        message += GetDescriptorSemanticName(req.semantic);
        message += ", name=";
        message += (req.name && *req.name) ? req.name : "<unnamed>";
        message += ", def=";
        message += definition.definition_name.empty() ? "<unnamed>" : definition.definition_name;
        diagnostics.push_back(std::move(message));
    }

    return diagnostics.empty();
}

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial — Compositor 模板完整 GLSL → ShaderProgramBuildSpec
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config)
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

    const PrimitiveType primitive_type = config.primitive_type;
    const uint32_t shader_stage_bits = config.shader_stage_flag_bits != 0 ? config.shader_stage_flag_bits : uint32_t(ShaderStage::VertexFragment);

    const bool infer_has_l2w = HasDescriptorSemantic(def, DescriptorSemantic::LocalToWorld);
    const bool with_local_to_world = infer_has_l2w;

    // ─────────────────────────────────────────────────────────────
    // Step 2: Create ShaderProgramBuildSpec
    // ─────────────────────────────────────────────────────────────

    ShaderProgramBuildSpec *mci = new ShaderProgramBuildSpec(primitive_type, shader_stage_bits, with_local_to_world);
    if (profile)
        mci->SetDevice(profile);

    auto FailAfterMci = [&](const char *reason) -> ShaderProgramBuildSpec *
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            reason ? reason : "<unknown>");
        delete mci;
        return nullptr;
    };

    uint32_t material_ssbo_stage_bits = uint32_t(ShaderStage::Fragment);

    // ─────────────────────────────────────────────────────────────
    // Step 3: Add Descriptors from FixedDescriptorEntry[]
    // When config.ssbo_slot_decls is provided, material SSBO entries are
    // generated from it; any baked single-slot entries in def are skipped.
    // ─────────────────────────────────────────────────────────────

    const bool use_slot_decls = config.ssbo_slot_decls && !config.ssbo_slot_decls->empty();

#if defined(_DEBUG)
    {
        std::vector<std::string> e0_diagnostics;
        if (!ValidateE0DescriptorShapeBaseline(def, config, e0_diagnostics))
        {
            for (const auto &diag : e0_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][E0Baseline] material=%s: %s\n",
                    def.name ? def.name : "<unnamed>",
                    diag.c_str());
            }
            return FailAfterMci("E0 descriptor shape baseline validation failed");
        }
    }
#endif

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const FixedDescriptorEntry &entry = def.descriptor_entries[i];

        // Skip baked material-SSBO entries when slot_decls takes over.
        if (use_slot_decls && entry.semantic == DescriptorSemantic::MaterialSSBOSlotData)
            continue;

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
            case DescriptorSemantic::MaterialSSBOSlotData:
                material_ssbo_stage_bits = stage_bits;
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
            case DescriptorSemantic::MaterialSSBOSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialTextureLayerTable:
                mci->AddSSBOStruct(stage_bits, SBS_MaterialTextureLayerRows);
                break;
            case DescriptorSemantic::MaterialSSBOIndexTable:
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

    if (use_slot_decls)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.ssbo_slot_decls->size()); ++i)
        {
            if (!AddMaterialSSBOSlotDescriptor(*mci, (*config.ssbo_slot_decls)[i], i, material_ssbo_stage_bits))
                return FailAfterMci("failed to add declared material ssbo slot descriptor");
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 6: Set complete GLSL (bypass ProcXXX pipeline)
    // ─────────────────────────────────────────────────────────────

    constexpr uint32_t texture_slot_range_size = static_cast<uint32_t>(TextureSlot::RANGE_SIZE);

    std::string binding_preamble;
    binding_preamble += "#define TEXTURE_SLOT_RANGE_SIZE " + std::to_string(texture_slot_range_size) + "u\n";

    // Inject per-material SSBO slot count when declared via ssbo_slot_decls.
    if (use_slot_decls)
        binding_preamble += "#define MTL_SSBO_SLOT_COUNT " + std::to_string(config.ssbo_slot_decls->size()) + "u\n";

    // GLSL requires #version to be the very first token.
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
    // Step 6b: Build MaterialResourceLayout from descriptor entries.
    // When ssbo_slot_decls is provided, material SSBO entries are
    // generated from it and merged with the rest of def.descriptor_entries.
    // ─────────────────────────────────────────────────────────────

    MaterialResourceLayout material_resource_layout;
    if (use_slot_decls)
    {
        // Build augmented list: base entries (without legacy single-slot material SSBO) + slot_decls entries.
        std::vector<FixedDescriptorEntry> augmented;
        // Names from ssbo_slot_decls are runtime strings; store them here to ensure lifetime.
        std::vector<std::string> augmented_names;
        augmented.reserve(def.descriptor_entry_count + config.ssbo_slot_decls->size());
        augmented_names.reserve(config.ssbo_slot_decls->size());
        for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
        {
            if (def.descriptor_entries[i].semantic != DescriptorSemantic::MaterialSSBOSlotData)
                augmented.push_back(def.descriptor_entries[i]);
        }
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.ssbo_slot_decls->size()); ++i)
        {
            const MaterialSSBOSlotDecl &decl = (*config.ssbo_slot_decls)[i];
            augmented_names.push_back(decl.name);  // owned copy guarantees lifetime
            FixedDescriptorEntry e{};
            e.set_type      = DescriptorSetType::Material;
            e.kind          = DescriptorKind::SSBO;
            e.stage_flags   = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
            e.name          = augmented_names.back().c_str();  // stable pointer into owned string
            e.struct_name   = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            e.glsl_type     = nullptr;
            e.semantic      = DescriptorSemantic::MaterialSSBOSlotData;
            e.texture_slot  = TextureSlot::BaseColor;
            e.ssbo_slot     = i;
            e.ssbo_type     = decl.ssbo_type;
            e.semantic_layer = GetDescriptorSemanticLayerByKind(e.kind);
            e.ssbo_id       = MakeRecipeSSBOId(i);
            augmented.push_back(e);
        }
        material_resource_layout = BuildMaterialResourceLayout(augmented.data(), static_cast<uint32_t>(augmented.size()));

        // Fix up dangling name pointers: BuildMaterialResourceLayout copied the const char* pointers.
        // Any requirement whose name pointed into augmented_names must be re-pointed via owned_name.
        for (auto &req : material_resource_layout.requirements)
        {
            if (req.semantic != DescriptorSemantic::MaterialSSBOSlotData)
                continue;
            // req.ssbo_slot is the slot index, which matches augmented_names order.
            if (req.ssbo_slot < static_cast<uint32_t>(augmented_names.size()))
            {
                req.owned_name   = augmented_names[req.ssbo_slot];
                req.name         = req.owned_name.c_str();
            }
        }
    }
    else
    {
        material_resource_layout = BuildMaterialResourceLayout(def.descriptor_entries, def.descriptor_entry_count);
    }

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

    if (config.material_definition)
    {
        std::vector<std::string> capability_diagnostics;
        if (!ValidateDefinitionCapabilitySubset(*config.material_definition, material_resource_layout, capability_diagnostics))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][DefinitionCapability] material=%s: %s\n",
                    def.name ? def.name : "<unnamed>",
                    diag.c_str());
            }
            return FailAfterMci("Definition capability subset validation failed");
        }
    }

    mci->SetMaterialResourceLayout(material_resource_layout);

    // ─────────────────────────────────────────────────────────────
    // Step 7: Compile directly → SPV
    // ─────────────────────────────────────────────────────────────

    if (!mci->CreateShaderDirect())
    {
        return FailAfterMci("CreateShaderDirect() failed (check GLSLCompiler log)");
    }

    return mci;
}

}  // namespace hgl::graph::mtl
