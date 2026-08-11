/// MaterialCompiler.cpp — canonical material input compiler
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderProgramArtifactBuilder.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include "common/DescriptorBuilderCommon.h"
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::shadergen {
    using namespace hgl::graph::mtl;

bool FinalizeShaderProgramBuildSpec(
    ShaderProgramBuildSpec *build_spec)
{
    if (!build_spec)
        return false;

    ShaderArtifactStore *artifact_store =
        build_spec->GetArtifactStore();
    if (artifact_store
     && (!build_spec->HasProgramLink()
      || !build_spec->HasProgramArtifactMetadata()))
        return false;

    bool cache_hit = false;
    if (artifact_store)
    {
        const auto &link = build_spec->GetProgramLink();
        ValueArray<hgl::uint8> cached_vertex;
        ValueArray<hgl::uint8> cached_fragment;
        cache_hit = artifact_store->LoadProgramArtifacts(
            link,
            build_spec->GetProgramArtifactMetadata(),
            cached_vertex,
            cached_fragment);
        if (cache_hit)
        {
            ShaderCreateInfo *vertex =
                build_spec->GetStageShader(ShaderStage::Vertex);
            ShaderCreateInfo *fragment =
                build_spec->GetStageShader(ShaderStage::Fragment);
            cache_hit = vertex
                     && fragment
                     && vertex->SetCachedSPVData(
                            cached_vertex.GetData(),
                            cached_vertex.GetCount())
                     && fragment->SetCachedSPVData(
                            cached_fragment.GetData(),
                            cached_fragment.GetCount());
        }
    }

    if (!cache_hit
     && artifact_store
     && artifact_store->GetCacheMode() == ShaderCacheMode::ReadOnly)
        return false;

    if (!cache_hit && !build_spec->CreateShaderDirect())
        return false;

    if (!cache_hit && artifact_store)
    {
        const auto &link = build_spec->GetProgramLink();
        const ShaderCreateInfo *vertex =
            build_spec->GetStageShader(ShaderStage::Vertex);
        const ShaderCreateInfo *fragment =
            build_spec->GetStageShader(ShaderStage::Fragment);
        if (!vertex || !fragment
         || !artifact_store->SaveStageSPV(
                link.vertex_stage,
                vertex->GetSPVData(),
                vertex->GetSPVSize())
         || !artifact_store->SaveStageSPV(
                link.fragment_stage,
                fragment->GetSPVData(),
                fragment->GetSPVSize())
         || !artifact_store->SaveProgramMetadata(
                link,
                build_spec->GetProgramArtifactMetadata()))
            return false;
    }

    return true;
}

static bool CStrEq(const char *lhs, const char *rhs)
{
    // Guard against corrupted/non-string pointers from malformed descriptor entries.
    if (lhs && reinterpret_cast<uintptr_t>(lhs) < 0x10000u)
        return false;
    if (rhs && reinterpret_cast<uintptr_t>(rhs) < 0x10000u)
        return false;
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

static std::string BuildCodeModuleGLSL(const ShaderResourceManifest *manifest)
{
    if (!manifest || !manifest->IsValid())
        return {};

    std::string result;
    for (uint32 i = 0; i < manifest->code_module_count; ++i)
    {
        const GLSLCodeModuleDefinition *module =
            FindGLSLCodeModuleDefinition(manifest->code_modules[i]);
        if (!module || !module->glsl_code)
            continue;
        result += "\n// GLSLCodeModule: ";
        result += module->name ? module->name : "Unknown";
        result += "\n";
        result += module->glsl_code;
        result += "\n";
    }
    return result;
}

static bool HasDescriptorSemantic(
    const MaterialDescriptorContract &contract,
    const DescriptorSemantic semantic)
{
    for (const MaterialDescriptorContractEntry &entry :
         contract.entries)
    {
        if (entry.canonical.semantic == semantic)
            return true;
    }

    return false;
}

static bool IsTextureSlotDeclared(const MaterialDefinition &definition, const TextureSlot slot) noexcept
{
    for (const auto &decl : definition.texture_slot_decls)
    {
        if (decl.slot == slot)
            return true;
    }

    return false;
}

static bool AddMaterialDataSlotDescriptor(ShaderProgramBuildSpec &mci,
                                          const MaterialDataSlotDecl &decl,
                                          const uint32_t data_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialSSBOLayout(decl.ssbo_type, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!mci.AddStruct(struct_name, glsl_codes))
        return false;

    return mci.AddSSBO(stage_bits, DescriptorSetType::Material, struct_name, decl.name, int(data_slot));
}

static bool ValidateDefinitionCapabilitySubset(
    const MaterialDefinition &definition,
    const MaterialResourceLayout &layout,
    std::vector<std::string> &diagnostics,
    const ShaderResourceManifest *manifest)
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

        case DescriptorSemantic::MaterialDataSlotData:
            allowed = req.data_slot < definition.data_slot_decls.size();
            if (allowed)
                allowed = definition.data_slot_decls[req.data_slot].ssbo_type == req.ssbo_type;
            break;

        case DescriptorSemantic::MaterialTextureLayerTable:
            allowed = !definition.data_slot_decls.empty()
                   || definition.vertex_varying.emit_texture_layer_id;
            break;
        case DescriptorSemantic::MaterialDataIndexTable:
            allowed = !definition.data_slot_decls.empty()
                   || definition.vertex_varying.emit_data_index_id;
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

        if (!allowed && manifest && manifest->IsValid())
        {
            for (uint32 i = 0; i < manifest->ubo_count && !allowed; ++i)
            {
                UBODescriptorSemantic ubo_semantic{};
                if (TryGetUBODescriptorSemantic(req.semantic, ubo_semantic)
                 && manifest->ubos[i].semantic == ubo_semantic)
                    allowed = true;
            }
            for (uint32 i = 0; i < manifest->ssbo_count && !allowed; ++i)
            {
                const auto &ssbo = manifest->ssbos[i];
                if (req.semantic == DescriptorSemantic::MaterialDataSlotData
                 && req.data_slot == ssbo.data_slot
                 && req.ssbo_type == ssbo.ssbo_type
                 && CStrEq(req.name, ssbo.name))
                    allowed = true;
            }
            for (uint32 i = 0; i < manifest->texture_count && !allowed; ++i)
            {
                const auto &texture = manifest->textures[i];
                if (req.semantic == texture.semantic
                 && req.texture_slot == texture.slot
                 && CStrEq(req.name, texture.name))
                    allowed = true;
            }
            if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             && manifest->texture_layer_count > 0)
                allowed = true;

            // The mtl_data_index_rows table only exists to route instance IDs
            // to material data-slot SSBOs. If any material data-slot SSBO was
            // declared purely via provider manifest metadata (no matching
            // TOML [resources].ssbos entry), the index table requirement is
            // implied and must be accepted the same way.
            if (req.semantic == DescriptorSemantic::MaterialDataIndexTable
             && manifest->ssbo_count > 0)
                allowed = true;
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
    const MaterialCompilerInput &input,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config)
{
    if (vs_glsl.empty() || fs_glsl.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s: vs_glsl or fs_glsl is empty\n",
            input.debug_name ? input.debug_name : "<unnamed>");
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────
    // Step 1: Config
    // ─────────────────────────────────────────────────────────────

    const PrimitiveType primitive_type = config.primitive_type;
    if (input.primitive_type != primitive_type)
        return nullptr;
    const uint32_t shader_stage_bits = config.shader_stage_flag_bits != 0 ? config.shader_stage_flag_bits : uint32_t(ShaderStage::VertexFragment);

    MaterialDescriptorContract base_descriptor_contract{};
    if (config.descriptor_contract)
    {
        base_descriptor_contract = *config.descriptor_contract;
    }
    else if (!BuildMaterialDescriptorContract(
                input.descriptor_entries,
                input.descriptor_entry_count,
                base_descriptor_contract))
    {
        return nullptr;
    }

    const bool infer_has_l2w = HasDescriptorSemantic(
        base_descriptor_contract, DescriptorSemantic::LocalToWorld);
    const bool with_local_to_world = infer_has_l2w;

    // ─────────────────────────────────────────────────────────────
    // Step 2: Create ShaderProgramBuildSpec
    // ─────────────────────────────────────────────────────────────

    ShaderProgramBuildSpec *mci = new ShaderProgramBuildSpec(primitive_type, shader_stage_bits, with_local_to_world);
    if (profile)
        mci->SetDevice(profile);
    if (config.program_link)
        mci->SetProgramLink(*config.program_link);
    mci->SetArtifactStore(config.artifact_store);

    auto FailAfterMci = [&](const char *reason) -> ShaderProgramBuildSpec *
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            input.debug_name ? input.debug_name : "<unnamed>",
            reason ? reason : "<unknown>");
        delete mci;
        return nullptr;
    };

    uint32_t material_ssbo_stage_bits = uint32_t(ShaderStage::Fragment);
    if (config.merge_resource_manifest_material_slots
     && config.resource_manifest
     && config.resource_manifest->IsValid())
    {
        for (uint32_t i = 0; i < config.resource_manifest->ssbo_count; ++i)
            material_ssbo_stage_bits |= config.resource_manifest->ssbos[i].stage_flags;
    }

    // ─────────────────────────────────────────────────────────────
    // Step 3: Add Descriptors from FixedDescriptorEntry[]
    // Provider metadata contributes material SSBO slots to the same canonical
    // declaration list as the material definition.
    // ─────────────────────────────────────────────────────────────

    std::vector<MaterialDataSlotDecl> effective_data_slot_decls;
    if (config.data_slot_decls)
        effective_data_slot_decls = *config.data_slot_decls;
    if (config.merge_resource_manifest_material_slots
     && config.resource_manifest
     && config.resource_manifest->IsValid())
    {
        for (uint32_t i = 0; i < config.resource_manifest->ssbo_count; ++i)
        {
            const auto &ssbo = config.resource_manifest->ssbos[i];
            if (ssbo.data_slot > MaxMaterialDataSlotsPerMaterial)
                return FailAfterMci("provider material data slot exceeds the supported limit");
            if (ssbo.data_slot > effective_data_slot_decls.size())
                return FailAfterMci("provider material data slots must be contiguous");

            if (ssbo.data_slot == effective_data_slot_decls.size())
            {
                MaterialDataSlotDecl decl;
                decl.name = ssbo.name;
                decl.ssbo_type = ssbo.ssbo_type;
                effective_data_slot_decls.push_back(decl);
            }
            else
            {
                const auto &decl = effective_data_slot_decls[ssbo.data_slot];
                if (decl.name != ssbo.name || decl.ssbo_type != ssbo.ssbo_type)
                    return FailAfterMci("provider material data slot conflicts with definition");
            }
        }
    }

    const std::vector<MaterialDataSlotDecl> *data_slot_decls =
        effective_data_slot_decls.empty() ? nullptr : &effective_data_slot_decls;
    const bool use_slot_decls = data_slot_decls != nullptr;

    MaterialDescriptorContract effective_descriptor_contract{};
    if (!BuildEffectiveMaterialDescriptorContract(
            base_descriptor_contract,
            data_slot_decls,
            material_ssbo_stage_bits,
            effective_descriptor_contract))
        return FailAfterMci("invalid effective material descriptor contract");
    if (config.material_definition
     && !EnsureMaterialDescriptorContractVaryingResources(
            config.material_definition->vertex_varying,
            effective_descriptor_contract))
    {
        return FailAfterMci(
            "failed to add varying descriptor contract resources");
    }

    std::vector<FixedDescriptorEntry> descriptor_entries;
    if (!ConvertMaterialDescriptorContractToFixed(
            effective_descriptor_contract, descriptor_entries))
        return FailAfterMci("failed to adapt material descriptor contract");

    const uint32_t declared_material_data_slot_count = use_slot_decls ? static_cast<uint32_t>(data_slot_decls->size()) : 0u;
    if (use_slot_decls)
    {
        if (declared_material_data_slot_count > MaxMaterialDataSlotsPerMaterial)
            return FailAfterMci("material data slot count exceeds the supported limit");

        for (uint32_t i = 0; i < declared_material_data_slot_count; ++i)
        {
            const auto &decl = (*data_slot_decls)[i];
            if (!IsValidMaterialDataSlotName(decl.name))
                return FailAfterMci("invalid material data slot GLSL name");

            for (uint32_t j = 0; j < i; ++j)
            {
                if ((*data_slot_decls)[j].name == decl.name)
                    return FailAfterMci("duplicate material data slot GLSL name");
            }
        }
    }

    std::string primary_sampler_name;

    for (const FixedDescriptorEntry &entry : descriptor_entries)
    {
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
            case DescriptorSemantic::MaterialDataSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialColorPalette:
                mci->AddUBOStruct(stage_bits, SBS_ColorPalette);
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
            case DescriptorSemantic::MaterialDataSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialTextureLayerTable:
                if (use_slot_decls)
                {
                    if (!mci->AddStruct(SBS_MaterialTextureLayerRows.struct_name, ""))
                        return FailAfterMci("failed to add MaterialTextureLayerRows struct");
                    if (!mci->AddSSBO(stage_bits,
                                      DescriptorSetType::Material,
                                      SBS_MaterialTextureLayerRows.struct_name,
                                      SBS_MaterialTextureLayerRows.name,
                                      int(declared_material_data_slot_count + 1u)))
                    {
                        return FailAfterMci("failed to add MaterialTextureLayerRows SSBO");
                    }
                }
                else
                {
                    if (!mci->AddSSBOStruct(stage_bits, SBS_MaterialTextureLayerRows))
                        return FailAfterMci("failed to add MaterialTextureLayerRows SSBO");
                }
                break;
            case DescriptorSemantic::MaterialDataIndexTable:
                if (use_slot_decls)
                {
                    if (!mci->AddStruct(SBS_MaterialDataIndexRows.struct_name, ""))
                        return FailAfterMci("failed to add MaterialDataIndexRows struct");
                    if (!mci->AddSSBO(stage_bits,
                                      DescriptorSetType::Material,
                                      SBS_MaterialDataIndexRows.struct_name,
                                      SBS_MaterialDataIndexRows.name,
                                      int(declared_material_data_slot_count)))
                    {
                        return FailAfterMci("failed to add MaterialDataIndexRows SSBO");
                    }
                }
                else
                {
                    if (!mci->AddSSBOStruct(stage_bits, SBS_MaterialDataIndexRows))
                        return FailAfterMci("failed to add MaterialDataIndexRows SSBO");
                }
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
                if (primary_sampler_name.empty() && entry.name && *entry.name)
                    primary_sampler_name = entry.name;

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
        for (uint32_t i = 0; i < input.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = input.vertex_entries[i];
            vsc->AddInput(entry.format, entry.semantic);
        }
    }

    if (use_slot_decls)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(data_slot_decls->size()); ++i)
        {
            if (!AddMaterialDataSlotDescriptor(*mci, (*data_slot_decls)[i], i, material_ssbo_stage_bits))
                return FailAfterMci("failed to add declared material ssbo slot descriptor");
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 5a: Ensure index-table SSBOs for vertex-varying emissions.
    //
    // The descriptor builder calls EnsureMaterialDataIndexTable only when a
    // MaterialDataSlotData entry exists.  Compositor materials without data
    // slots may still emit fragDataIndexID / fragTextureLayerID (declared
    // as varyings in .material.toml).  Without the corresponding SSBO the VS
    // GLSL injection skips ResolveDataIndexID/ResolveTextureLayerID → compile
    // error.
    // ─────────────────────────────────────────────────────────────

    if (config.material_definition)
    {
        const auto &vv = config.material_definition->vertex_varying;

        auto HasDescriptorSemanticInDef = [&](DescriptorSemantic sem) -> bool
        {
            for (const FixedDescriptorEntry &entry : descriptor_entries)
                if (entry.semantic == sem)
                    return true;
            return false;
        };

        if (vv.emit_data_index_id
            && !HasDescriptorSemanticInDef(DescriptorSemantic::MaterialDataIndexTable))
        {
            mci->AddSSBOStruct(uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                               SBS_MaterialDataIndexRows);
        }

        if (vv.emit_texture_layer_id
            && !vv.texture_layer_id_uses_data_index
            && !HasDescriptorSemanticInDef(DescriptorSemantic::MaterialTextureLayerTable))
        {
            mci->AddSSBOStruct(uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                               SBS_MaterialTextureLayerRows);
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 6: Set complete GLSL (bypass ProcXXX pipeline)
    // ─────────────────────────────────────────────────────────────

    constexpr uint32_t texture_slot_range_size = static_cast<uint32_t>(TextureSlot::RANGE_SIZE);

    std::string binding_preamble;
    binding_preamble += "#define TEXTURE_SLOT_RANGE_SIZE " + std::to_string(texture_slot_range_size) + "u\n";

    auto AppendDescriptorBindingDefine = [&](const char *macro_name, const ShaderDescriptor *sd)
    {
        if (!macro_name || !sd || sd->set < 0 || sd->binding < 0)
            return;
        binding_preamble += "#define ";
        binding_preamble += macro_name;
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->set);
        binding_preamble += "\n";
        binding_preamble += "#define ";
        binding_preamble += std::string(macro_name).replace(std::string(macro_name).find("_SET"), 4, "_BINDING");
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->binding);
        binding_preamble += "\n";
    };

    const MaterialDescriptorInfo &descriptor_info = mci->GetDescriptorInfo();

    // 行表绑定（mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows）不再注入
    // set/binding 宏：声明由下方 index table 生成逻辑依据 descriptor_info 直接以
    // layout(set=.., binding=..) 写出（统一声明生成，不再写死在 .glsl）。
    AppendDescriptorBindingDefine("L2W_SET", descriptor_info.GetSSBO(SBS_LocalToWorld.name));
    AppendDescriptorBindingDefine("VIEWPORT_SET", descriptor_info.GetUBO(SBS_ViewportInfo.name));
    AppendDescriptorBindingDefine("CAMERA_SET", descriptor_info.GetUBO(SBS_CameraInfo.name));
    AppendDescriptorBindingDefine("SKY_SET", descriptor_info.GetUBO(SBS_SkyInfo.name));
    AppendDescriptorBindingDefine(
        "SKY_CUBEMAP_SET", descriptor_info.GetTextureSampler("SkyCubemap"));

    const TextureSamplerDescriptor *primary_sampler = nullptr;
    if (!primary_sampler_name.empty())
        primary_sampler = descriptor_info.GetTextureSampler(primary_sampler_name.c_str());

    if (!primary_sampler)
        primary_sampler = descriptor_info.GetTextureSampler("TextureBaseColor");

    if (!primary_sampler && config.material_definition)
    {
        for (const auto &slot_decl : config.material_definition->texture_slot_decls)
        {
            if (!slot_decl.name || !*slot_decl.name)
                continue;

            primary_sampler = descriptor_info.GetTextureSampler(slot_decl.name);
            if (primary_sampler)
                break;
        }
    }

    if (primary_sampler)
    {
        if (primary_sampler->set >= 0 && primary_sampler->binding >= 0)
        {
            binding_preamble += "#define TEX_SET " + std::to_string(primary_sampler->set) + "\n";
            binding_preamble += "#define TEX_BINDING " + std::to_string(primary_sampler->binding) + "\n";
        }
    }

    // ── Material SSBO GLSL 声明 ─────────────────────────────────────────────
    // 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
    // 统一由此处依据 data_slot_decls 生成并注入 Fragment 阶段。
    std::string material_ssbo_decls;
    std::string material_slot_macros;

    if (use_slot_decls && data_slot_decls && !data_slot_decls->empty())
    {
        std::vector<std::string> emitted_struct_names;
        std::vector<std::string> emitted_buffer_names;
        for (uint32_t i = 0; i < static_cast<uint32_t>(data_slot_decls->size()); ++i)
        {
            const MaterialDataSlotDecl &decl = (*data_slot_decls)[i];
            const ShaderDescriptor *sd = descriptor_info.GetSSBO(decl.name.c_str());
            if (!sd || sd->set < 0 || sd->binding < 0)
                return FailAfterMci("material ssbo descriptor unresolved for GLSL generation");

            const char *struct_name  = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(decl.ssbo_type);
            if (!struct_name || !struct_codes)
                return FailAfterMci("unsupported material ssbo type for GLSL generation");

            std::string buffer_name(struct_name);
            const size_t name_len = buffer_name.size();
            if (name_len > 4 && buffer_name.compare(name_len - 4, 4, "Data") == 0)
                buffer_name.resize(name_len - 4);
            buffer_name += "Buffer";

            for (uint32_t suffix = 1;; ++suffix)
            {
                bool used = false;
                for (const auto &used_name : emitted_buffer_names)
                {
                    if (used_name == buffer_name)
                    {
                        used = true;
                        break;
                    }
                }
                if (!used)
                    break;
                buffer_name = std::string(struct_name) + "Buffer_" + std::to_string(suffix);
            }
            emitted_buffer_names.push_back(buffer_name);

            bool struct_emitted = false;
            for (const auto &emitted_name : emitted_struct_names)
            {
                if (emitted_name == struct_name)
                {
                    struct_emitted = true;
                    break;
                }
            }

            if (!struct_emitted)
            {
                emitted_struct_names.emplace_back(struct_name);
                material_ssbo_decls += "struct ";
                material_ssbo_decls += struct_name;
                material_ssbo_decls += "\n{\n";

                // 规范化字段文本：去掉行首空白、统一 4 空格缩进。
                std::string line;
                const char *p = struct_codes;
                auto FlushFieldLine = [&]()
                {
                    size_t start = 0;
                    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
                        ++start;
                    if (start < line.size())
                    {
                        material_ssbo_decls += "    ";
                        material_ssbo_decls.append(line, start, line.size() - start);
                        material_ssbo_decls += '\n';
                    }
                    line.clear();
                };
                for (; *p; ++p)
                {
                    if (*p == '\n')
                        FlushFieldLine();
                    else
                        line += *p;
                }
                FlushFieldLine();

                material_ssbo_decls += "};\n";
            }

            material_ssbo_decls += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
            material_ssbo_decls += buffer_name;
            material_ssbo_decls += " {\n    ";
            material_ssbo_decls += struct_name;
            material_ssbo_decls += " data[];\n} ";
            material_ssbo_decls += decl.name;
            material_ssbo_decls += ";\n";
        }

        material_slot_macros += "#define MTL_DATA_SLOT_COUNT "
            + std::to_string(data_slot_decls->size()) + "u\n";
        material_slot_macros += "#define MTL_DATA_INDEX_ROW_STRIDE "
            + std::to_string(MaterialDataIndexRowStride) + "u\n";
        for (uint32_t i = 0; i < static_cast<uint32_t>(data_slot_decls->size()); ++i)
        {
            material_slot_macros += "#define MTL_DATA_SLOT_";
            material_slot_macros += std::to_string(i);
            material_slot_macros += " ";
            material_slot_macros += (*data_slot_decls)[i].name;
            material_slot_macros += "\n";
        }
        material_slot_macros += "#define MTL_DATA ";
        material_slot_macros += (*data_slot_decls)[0].name;
        material_slot_macros += "\n";
    }

    // ── Instance index table SSBO GLSL 声明 ──────────────────────────────────
    // mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows 的 buffer
    // 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
    // descriptor_info 生成注入：VS 阶段提供 l2w_index_rows / mtl_data_index_rows
    //（含 ResolveTransformID / ResolveDataIndexID），FS 阶段提供
    // mtl_texture_layer_rows（bindless GetTextureHandle 行表，仅需 buffer 声明）。
    struct IndexTableSpec
    {
        const char *buffer_name;
        const char *var_name;
        const char *resolve_func;   // 为空则仅生成 buffer 声明
        bool slot_aware = false;
    };

    auto AppendIndexTableDecl = [](std::string &out, const ShaderDescriptor *sd, const IndexTableSpec &spec)
    {
        if (!sd || sd->set < 0 || sd->binding < 0)
            return;

        out += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
        out += spec.buffer_name;
        out += " { uint values[]; } ";
        out += spec.var_name;
        out += ";\n";

        if (spec.resolve_func)
        {
            if (spec.slot_aware)
            {
                out += "uint ";
                out += spec.resolve_func;
                out += "(uint iid, uint data_slot) { return ";
                out += spec.var_name;
                out += ".values[iid * MTL_DATA_INDEX_ROW_STRIDE + data_slot]; }\n";
            }
            out += "uint ";
            out += spec.resolve_func;
            out += "(uint iid) { return ";
            if (spec.slot_aware)
            {
                out += spec.resolve_func;
                out += "(iid, 0u); }\n";
            }
            else
            {
                out += spec.var_name;
                out += ".values[iid]; }\n";
            }
        }
    };

    std::string vs_index_table_decls;
    std::string fs_index_table_decls;
    const uint32_t material_data_slot_count =
        use_slot_decls ? static_cast<uint32_t>(data_slot_decls->size()) : 1u;
    vs_index_table_decls = "#define MTL_DATA_SLOT_COUNT "
        + std::to_string(material_data_slot_count) + "u\n"
        + "#define MTL_DATA_INDEX_ROW_STRIDE "
        + std::to_string(MaterialDataIndexRowStride) + "u\n";

    AppendIndexTableDecl(vs_index_table_decls, descriptor_info.GetSSBO(SBS_LocalToWorldIndexRows.name),
                         { "LocalToWorldIndexRows", "l2w_index_rows", "ResolveTransformID" });
    AppendIndexTableDecl(vs_index_table_decls, descriptor_info.GetSSBO(SBS_MaterialDataIndexRows.name),
                         { "DataIndexRows", "mtl_data_index_rows", "ResolveDataIndexID", true });
    // VS 阶段提供 mtl_texture_layer_rows 的 buffer 声明 + ResolveTextureLayerID
    //（供 emit_texture_layer_id 且 texture_layer_id_uses_data_index=false 的材质使用）。
    AppendIndexTableDecl(vs_index_table_decls, descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name),
                         { "TextureLayerRows", "mtl_texture_layer_rows", "ResolveTextureLayerID" });
    // FS 阶段仅需 buffer 声明（bindless GetTextureHandle 行表），无需 resolve 函数。
    AppendIndexTableDecl(fs_index_table_decls, descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name),
                         { "TextureLayerRows", "mtl_texture_layer_rows", nullptr });

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

    auto InsertBeforeSurfaceFunction = [](const std::string &glsl, const std::string &inject) -> std::string
    {
        if (inject.empty())
            return glsl;
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        auto pos = glsl.find(marker);
        if (pos == std::string::npos)
        {
            const auto include_pos = glsl.find("#include \"surface/");
            pos = include_pos;
        }
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos) + inject + "\n" + glsl.substr(pos);
    };

    std::string vs_final = InsertAfterVersionLine(vs_glsl, binding_preamble + vs_index_table_decls);
    std::string fs_final = InsertAfterVersionLine(
        fs_glsl, binding_preamble + fs_index_table_decls + material_ssbo_decls + material_slot_macros);
    const std::string code_module_glsl = BuildCodeModuleGLSL(config.resource_manifest);
    if (!code_module_glsl.empty())
    {
        if (fs_glsl.find("#include SURFACE_FUNCTION_FILE") == std::string::npos
         && fs_glsl.find("#include \"surface/") == std::string::npos)
            fs_final = InsertAfterVersionLine(fs_final, code_module_glsl);
        else
            fs_final = InsertBeforeSurfaceFunction(fs_final, code_module_glsl);
    }

    ShaderCreateInfoVertex   *vert = mci->GetVertexShader();
    ShaderCreateInfo         *frag = mci->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);

    // ─────────────────────────────────────────────────────────────
    // Step 6b: Build MaterialResourceLayout from descriptor entries.
    // When data_slot_decls is provided, material SSBO entries are
    // generated from it and merged with the canonical descriptor entries.
    // ─────────────────────────────────────────────────────────────

    MaterialResourceLayout material_resource_layout;
    if (!BuildMaterialResourceLayoutFromDescriptorContract(
            effective_descriptor_contract,
            material_resource_layout))
        return FailAfterMci("descriptor contract/layout build failed");

    if (config.material_definition)
        descriptor_builder_common::ApplyMaterialDefinitionTexturePolicy(
            *config.material_definition, material_resource_layout);

    std::vector<std::string> contract_diagnostics;
    if (!ValidateMaterialResourceLayout(material_resource_layout, contract_diagnostics))
    {
        for (const auto &diag : contract_diagnostics)
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial][MaterialResourceLayout] material=%s: %s\n",
                input.debug_name ? input.debug_name : "<unnamed>",
                diag.c_str());
        }
        return FailAfterMci("MaterialResourceLayout validation failed");
    }

    if (config.material_definition)
    {
        const MaterialDefinition &material_definition = *config.material_definition;
        std::vector<std::string> capability_diagnostics;
        if (!ValidateDefinitionCapabilitySubset(
                material_definition, material_resource_layout,
                capability_diagnostics, config.resource_manifest))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][DefinitionCapability] material=%s: %s\n",
                    input.debug_name ? input.debug_name : "<unnamed>",
                    diag.c_str());
            }
            return FailAfterMci("Definition capability subset validation failed");
        }
    }

    mci->SetMaterialResourceLayout(material_resource_layout);
    if (mci->HasProgramLink())
    {
        ShaderProgramArtifactMetadata metadata{};
        if (!BuildShaderProgramArtifactMetadata(
                profile, *mci, metadata))
            return FailAfterMci(
                "failed to build ShaderProgram artifact metadata");
        mci->SetProgramArtifactMetadata(metadata);
    }

    // ─────────────────────────────────────────────────────────────
    // Step 7: Compile directly → SPV
    // ─────────────────────────────────────────────────────────────

    if (config.generate_only)
        return mci;

    if (!FinalizeShaderProgramBuildSpec(mci))
        return FailAfterMci(
            "FinalizeShaderProgramBuildSpec() failed");

    return mci;
}

}  // namespace hgl::graph::shadergen
