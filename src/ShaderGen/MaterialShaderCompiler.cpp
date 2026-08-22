/// MaterialShaderCompiler.cpp — canonical material input compiler
///
/// 流程：
///   1. 从 SerializedDescriptorEntry[] 构建 DescriptorSetLayoutAllocator（描述符布局）
///   2. 从 SerializedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfoVertex.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/GLSLCodeModule.h>
#include "common/DescriptorBuilderCommon.h"
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {
    using namespace hgl::graph::mtl;

bool FinalizeShaderBuildContext(
    ShaderBuildContext *build_spec)
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

static std::string BuildCodeModuleGLSL(const ModuleResourceManifest *manifest)
{
    if (!manifest || !manifest->IsValid())
        return {};

    const auto &module_registry = mtl::GetGLSLCodeModuleRegistry();
    std::string result;
    for (uint32 i = 0; i < manifest->code_module_count; ++i)
    {
        const GLSLCodeModuleDefinition *module =
            module_registry.FindByName(manifest->code_module_names[i]);
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

std::string BuildSamplerMacros(const std::vector<std::string> &sampler_names)
{
    std::string macros;
    for (const auto &name : sampler_names)
    {
        if (name.empty())
            continue;
        const uint32_t idx = SamplerPresetLibrary::Instance().GetIndex(name.c_str());
        if (idx == ~0u)
        {
            // sampler.toml 无此名字——不生成宏，shader 编译会因未定义
            // 宏显式失败（vs 静默错位成 Nearest）
            GLogError(u8"[MaterialShaderCompiler] sampler preset not found: %s — "
                      u8"check sampler.toml ordering",
                      name.c_str());
            continue;
        }
        macros += "#define ";
        macros += name;
        macros += "Sampler ";
        macros += std::to_string(idx);
        macros += "u\n";
    }
    return macros;
}

static bool HasDescriptorSemantic(
    const DescriptorContract &contract,
    const DescriptorSemantic semantic)
{
    for (const DescriptorContractEntry &entry :
         contract.entries)
    {
        if (entry.canonical.semantic == semantic)
            return true;
    }

    return false;
}

static bool AddMaterialDataSlotDescriptor(ShaderBuildContext &ctx,
                                          const DataSlotDeclaration &decl,
                                          const uint32_t data_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialSSBOLayout(decl.ssbo_type, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!ctx.AddStruct(struct_name, glsl_codes))
        return false;

    return ctx.AddSSBOMtlData(stage_bits, struct_name, decl.name, int(data_slot));
}

static bool ValidateDefinitionCapabilitySubset(
    const MaterialDefinition &definition,
    const ShaderResourceSchema &layout,
    std::vector<std::string> &diagnostics,
    const ModuleResourceManifest *manifest)
{
    diagnostics.clear();

    for (const auto &req : layout.resources)
    {
        bool allowed = false;

        switch (req.semantic)
        {
        case DescriptorSemantic::ViewportInfo:
            allowed = HasUBORequirement(definition, DescriptorSemantic::ViewportInfo);
            break;
        case DescriptorSemantic::CameraInfo:
            allowed = HasUBORequirement(definition, DescriptorSemantic::CameraInfo);
            break;
        case DescriptorSemantic::SkyInfo:
            allowed = HasUBORequirement(definition, DescriptorSemantic::SkyInfo);
            break;
        case DescriptorSemantic::MaterialColorPalette:
            allowed = HasUBORequirement(definition, DescriptorSemantic::MaterialColorPalette);
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
            allowed = !definition.texture_slot_decls.empty();
            break;
        case DescriptorSemantic::MaterialDataIndexTable:
            allowed = !definition.data_slot_decls.empty()
                   || definition.vertex_varying.emit_data_index_id;
            break;

        // 顶点数据 SSBO（MeshShader 方向）：顶点 SSBO 语义无条件允许（顶点输入统一为 SSBO）
        case DescriptorSemantic::VertexPosition:
        case DescriptorSemantic::VertexUV:
        case DescriptorSemantic::VertexNTB:
        case DescriptorSemantic::VertexJoint:
        case DescriptorSemantic::VertexColor:
        case DescriptorSemantic::VertexLuminance:
        case DescriptorSemantic::VertexTransformID:
        case DescriptorSemantic::VertexIndex:
            allowed = true;
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
                if (manifest->ubos[i].semantic == req.semantic)
                    allowed = true;
            }
            for (uint32 i = 0; i < manifest->ssbo_count && !allowed; ++i)
            {
                const auto &ssbo = manifest->ssbos[i];
                if (req.semantic == DescriptorSemantic::MaterialDataSlotData
                 && req.data_slot == ssbo.data_slot
                 && req.ssbo_type == ssbo.ssbo_type
                 && CStrEq(req.name.c_str(), ssbo.name))
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
        message += req.name.empty() ? "<unnamed>" : req.name;
        message += ", def=";
        message += definition.definition_name.empty() ? "<unnamed>" : definition.definition_name;
        diagnostics.push_back(std::move(message));
    }

    return diagnostics.empty();
}

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial — Compositor 模板完整 GLSL → ShaderBuildContext
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

ShaderBuildContext *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
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

    DescriptorContract base_descriptor_contract{};
    if (config.descriptor_contract)
    {
        base_descriptor_contract = *config.descriptor_contract;
    }
    else if (!BuildDescriptorContract(
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
    // Step 2: Create ShaderBuildContext
    // ─────────────────────────────────────────────────────────────

    ShaderBuildContext *ctx = new ShaderBuildContext(primitive_type, shader_stage_bits, with_local_to_world);
    if (profile)
        ctx->SetDevice(profile);
    if (config.program_link)
        ctx->SetProgramLink(*config.program_link);
    ctx->SetArtifactStore(config.artifact_store);

    auto FailAfterBuild = [&](const char *reason) -> ShaderBuildContext *
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            input.debug_name ? input.debug_name : "<unnamed>",
            reason ? reason : "<unknown>");
        delete ctx;
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
    // Step 3: Add Descriptors from SerializedDescriptorEntry[]
    // Provider metadata contributes material SSBO slots to the same canonical
    // declaration list as the material definition.
    // ─────────────────────────────────────────────────────────────

    std::vector<DataSlotDeclaration> effective_data_slot_decls;
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
                return FailAfterBuild("provider material data slot exceeds the supported limit");
            if (ssbo.data_slot > effective_data_slot_decls.size())
                return FailAfterBuild("provider material data slots must be contiguous");

            if (ssbo.data_slot == effective_data_slot_decls.size())
            {
                DataSlotDeclaration decl;
                decl.name = ssbo.name;
                decl.ssbo_type = ssbo.ssbo_type;
                effective_data_slot_decls.push_back(decl);
            }
            else
            {
                const auto &decl = effective_data_slot_decls[ssbo.data_slot];
                if (decl.name != ssbo.name || decl.ssbo_type != ssbo.ssbo_type)
                    return FailAfterBuild("provider material data slot conflicts with definition");
            }
        }
    }

    const std::vector<DataSlotDeclaration> *data_slot_decls =
        effective_data_slot_decls.empty() ? nullptr : &effective_data_slot_decls;
    const bool use_slot_decls = data_slot_decls != nullptr;

    DescriptorContract effective_descriptor_contract{};
    if (!BuildEffectiveDescriptorContract(
            base_descriptor_contract,
            data_slot_decls,
            material_ssbo_stage_bits,
            effective_descriptor_contract))
        return FailAfterBuild("invalid effective material descriptor contract");
    if (config.material_definition
     && !EnsureDescriptorContractVaryingResources(
            config.material_definition->vertex_varying,
            effective_descriptor_contract))
    {
        return FailAfterBuild(
            "failed to add varying descriptor contract resources");
    }

    std::vector<SerializedDescriptorEntry> descriptor_entries;
    if (!ConvertDescriptorContractToFixed(
            effective_descriptor_contract, descriptor_entries))
        return FailAfterBuild("failed to adapt material descriptor contract");

    const uint32_t declared_material_data_slot_count = use_slot_decls ? static_cast<uint32_t>(data_slot_decls->size()) : 0u;
    if (use_slot_decls)
    {
        if (declared_material_data_slot_count > MaxMaterialDataSlotsPerMaterial)
            return FailAfterBuild("material data slot count exceeds the supported limit");

        for (uint32_t i = 0; i < declared_material_data_slot_count; ++i)
        {
            const auto &decl = (*data_slot_decls)[i];
            if (!IsValidMaterialDataSlotName(decl.name))
                return FailAfterBuild("invalid material data slot GLSL name");

            for (uint32_t j = 0; j < i; ++j)
            {
                if ((*data_slot_decls)[j].name == decl.name)
                    return FailAfterBuild("duplicate material data slot GLSL name");
            }
        }
    }

    for (const SerializedDescriptorEntry &entry : descriptor_entries)
    {
        const uint32_t stage_bits = entry.stage_flags;

        switch (entry.kind)
        {
        case DescriptorKind::UBO:
            switch (entry.semantic)
            {
            // 注：Scene UBO（ViewportInfo/CameraInfo/SkyInfo/ColorPalette）已全局化（P1/P1-2a），
            //     不再进入 per-material 分配器（desc_manager/finalize/绑定均跳过 Scene），
            //     GLSL 声明与 set/binding 由全局集与宏注入保证。
            case DescriptorSemantic::ViewportInfo:
            case DescriptorSemantic::CameraInfo:
            case DescriptorSemantic::SkyInfo:
            case DescriptorSemantic::MaterialColorPalette:
                break;
            case DescriptorSemantic::LocalToWorld:
                ctx->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::MaterialDataSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            default:
                break;
            }
            break;

        case DescriptorKind::SSBO:
            switch (entry.semantic)
            {
            case DescriptorSemantic::LocalToWorld:
                ctx->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::LocalToWorldIndexTable:
                ctx->AddSSBOStruct(stage_bits, SBS_LocalToWorldIndexRows);
                break;
            case DescriptorSemantic::MaterialDataSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialTextureLayerTable:
                if (!ctx->AddStruct(SBS_MaterialTextureLayerRows.struct_name, ""))
                    return FailAfterBuild("failed to add MaterialTextureLayerRows struct");
                // 行表 binding 统一为「数据槽数 + 行表序号」：无 data slot 时 declared 计数为 0，
                // 同一公式覆盖 use_slot_decls 两种路径，避免特例字面量。
                // P1-2c：mtl_data_index_rows 已迁出 Material 集，texture_layer_rows
                // 紧随数据槽之后（binding = N），不再 +1。
                if (!ctx->AddSSBOTextureLayer(stage_bits, int(declared_material_data_slot_count)))
                {
                    return FailAfterBuild("failed to add MaterialTextureLayerRows SSBO");
                }
                break;
            case DescriptorSemantic::MaterialDataIndexTable:
                if (!ctx->AddStruct(SBS_MaterialDataIndexRows.struct_name, ""))
                    return FailAfterBuild("failed to add MaterialDataIndexRows struct");
                // P1-2c：mtl_data_index_rows 迁至 PerObject 集，binding 由固定常量表
                // kPerObjectBinding* 确定（走固定名路径，与 l2w_index_rows 同构）。
                if (!ctx->AddSSBOMtlIndex(stage_bits))
                {
                    return FailAfterBuild("failed to add MaterialDataIndexRows SSBO");
                }
                break;
            // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）——
            // PerObject 集固定 binding（kPerObjectBindingVertex*），走固定名路径
            case DescriptorSemantic::VertexPosition:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexPosition))
                    return FailAfterBuild("failed to add VertexPosition SSBO");
                break;
            case DescriptorSemantic::VertexUV:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexUV))
                    return FailAfterBuild("failed to add VertexUV SSBO");
                break;
            case DescriptorSemantic::VertexNTB:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexNTB))
                    return FailAfterBuild("failed to add VertexNTB SSBO");
                break;
            case DescriptorSemantic::VertexJoint:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexJoint))
                    return FailAfterBuild("failed to add VertexJoint SSBO");
                break;
            case DescriptorSemantic::VertexColor:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexColor))
                    return FailAfterBuild("failed to add VertexColor SSBO");
                break;
            case DescriptorSemantic::VertexLuminance:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexLuminance))
                    return FailAfterBuild("failed to add VertexLuminance SSBO");
                break;
            case DescriptorSemantic::VertexTransformID:
                if (!ctx->AddSSBOVertex(stage_bits, SBS_VertexTransformID))
                    return FailAfterBuild("failed to add VertexTransformID SSBO");
                break;
            case DescriptorSemantic::VertexIndex:
                if (!ctx->AddSSBOVertexIndex(stage_bits))
                    return FailAfterBuild("failed to add VertexIndex SSBO");
                break;
            default:
                break;
            }
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 4: Add Vertex Inputs from SerializedVertexEntry[]
    // ─────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex *vsc = ctx->GetVertexShader();
    if (vsc)
    {
        for (uint32_t i = 0; i < input.vertex_entry_count; ++i)
        {
            const SerializedVertexEntry &entry = input.vertex_entries[i];
            vsc->AddInput(entry.format, entry.semantic);
        }
    }

    if (use_slot_decls)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(data_slot_decls->size()); ++i)
        {
            if (!AddMaterialDataSlotDescriptor(*ctx, (*data_slot_decls)[i], i, material_ssbo_stage_bits))
                return FailAfterBuild("failed to add declared material ssbo slot descriptor");
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 5a: Ensure index-table SSBOs for vertex-varying emissions.
    //
    //     The descriptor builder calls EnsureMaterialDataIndexTable only when a
    // MaterialDataSlotData entry exists.  Compositor materials without data
    // slots may still emit fragDataIndexID (declared as a varying in
    // .material.toml).  Without the corresponding SSBO the VS GLSL injection
    // skips ResolveDataIndexID → compile error.
    // ─────────────────────────────────────────────────────────────

    if (config.material_definition)
    {
        const auto &vv = config.material_definition->vertex_varying;

        auto HasDescriptorSemanticInDef = [&](DescriptorSemantic sem) -> bool
        {
            for (const SerializedDescriptorEntry &entry : descriptor_entries)
                if (entry.semantic == sem)
                    return true;
            return false;
        };

        // P1-2c：mtl_data_index_rows 迁至 Transform 集，binding 由固定常量表确定，
        // 与 Step 3 的 MaterialDataIndexTable 分支同构。
        if (vv.emit_data_index_id
            && !HasDescriptorSemanticInDef(DescriptorSemantic::MaterialDataIndexTable))
        {
            ctx->AddStruct(SBS_MaterialDataIndexRows.struct_name, "");
            ctx->AddSSBO(uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                         SBS_MaterialDataIndexRows.set_type,
                         SBS_MaterialDataIndexRows.struct_name,
                         SBS_MaterialDataIndexRows.name);
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 6: Set complete GLSL (bypass ProcXXX pipeline)
    // ─────────────────────────────────────────────────────────────

    std::string binding_preamble;

    // 显式双宏名（_SET/_BINDING 成对传入）——不做字符串推导：
    // 推导依赖 "_SET" 子串存在，缺了即 std::out_of_range
    //
    // #ifndef 保护：模块 include 链可能提前引入 descriptor_macros.glsl
    // （其 L2W_SET=PER_OBJECT_SET 等宏体文本与这里不同——GLSL 重定义检查
    // 比较宏体文本，同值不同体也报错）。先定义者胜——当前布局下两者展开
    // 值一致（PER_OBJECT_SET=1=SBS set）；改 SBS 布局时需同步
    // descriptor_macros.glsl 的默认值。
    auto AppendDescriptorBindingDefine = [&](const char *set_macro, const char *binding_macro, const ShaderDescriptor *sd)
    {
        if (!set_macro || !binding_macro || !sd || sd->set < 0 || sd->binding < 0)
            return;
        binding_preamble += "#ifndef ";
        binding_preamble += set_macro;
        binding_preamble += "\n#define ";
        binding_preamble += set_macro;
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->set);
        binding_preamble += "\n#endif\n";
        binding_preamble += "#ifndef ";
        binding_preamble += binding_macro;
        binding_preamble += "\n#define ";
        binding_preamble += binding_macro;
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->binding);
        binding_preamble += "\n#endif\n";
    };

    const DescriptorSetLayoutAllocator &descriptor_info = ctx->GetDescriptorAllocator();

    // 行表绑定（mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows）不再注入
    // set/binding 宏：声明由下方 index table 生成逻辑依据 descriptor_info 直接以
    // layout(set=.., binding=..) 写出（统一声明生成，不再写死在 .glsl）。
    AppendDescriptorBindingDefine("L2W_SET", "L2W_BINDING", descriptor_info.GetSSBO(SBS_LocalToWorld.name));

    // ── Scene UBO（camera/sky/viewport/color_palette）已全局化（P1/P1-2a）：binding 号为
    //    P0/P1-2a 硬编码常量，不再从 per-material 分配器查询（Scene 不再进入 per-material 描述符集）。
    //    显式注入 _SET/_BINDING 宏，保证 shader ABI（descriptor_macros.glsl 默认值与此一致）。
    const int scene_set = int(DescriptorSetType::Scene);

    ShaderDescriptor sd_viewport, sd_camera, sd_sky, sd_color_palette;
    sd_viewport.set = scene_set; sd_viewport.binding = kSceneBindingViewport;
    sd_camera.set    = scene_set; sd_camera.binding    = kSceneBindingCamera;
    sd_sky.set       = scene_set; sd_sky.binding       = kSceneBindingSky;
    sd_color_palette.set = scene_set; sd_color_palette.binding = kSceneBindingColorPalette;

    AppendDescriptorBindingDefine("VIEWPORT_SET", "VIEWPORT_BINDING", &sd_viewport);
    AppendDescriptorBindingDefine("CAMERA_SET", "CAMERA_BINDING", &sd_camera);
    AppendDescriptorBindingDefine("SKY_SET", "SKY_BINDING", &sd_sky);
    AppendDescriptorBindingDefine("COLOR_PALETTE_SET", "COLOR_PALETTE_BINDING", &sd_color_palette);

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
            const DataSlotDeclaration &decl = (*data_slot_decls)[i];
            const ShaderDescriptor *sd = descriptor_info.GetSSBO(decl.name.c_str());
            if (!sd || sd->set < 0 || sd->binding < 0)
                return FailAfterBuild("material ssbo descriptor unresolved for GLSL generation");

            const char *struct_name  = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(decl.ssbo_type);
            if (!struct_name || !struct_codes)
                return FailAfterBuild("unsupported material ssbo type for GLSL generation");

            const char *const buffer_base =
                ssbo::GetMaterialSSBOBufferName(decl.ssbo_type);
            if (!buffer_base)
                return FailAfterBuild("material ssbo buffer name unsupported for GLSL generation");
            std::string buffer_name(buffer_base);

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

    // ── Sampler 预设宏（统一注册机制）──────────────────────────────────────
    // 遍历 MaterialDefinition.sampler_names，为每个名字生成 "#define <name>Sampler <idx>u"。
    // idx 经 SamplerPresetLibrary 查询（查不到保底 0），与运行时 binding=1 数组下标一致。
    std::string sampler_macros;
    if (config.material_definition)
        sampler_macros = BuildSamplerMacros(config.material_definition->sampler_names);

    // ── Instance index table SSBO GLSL 声明 ──────────────────────────────────
    // mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows 的 buffer
    // 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
    // descriptor_info 生成注入：VS 阶段提供 l2w_index_rows / mtl_data_index_rows
    //（含 ResolveTransformID / ResolveDataIndexID），FS 阶段提供
    // mtl_texture_layer_rows（named-slot TextureLayerRowsData，见下方注入）。
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
    // 无 data_slot_decls 时回退 1：MTL_DATA_SLOT_COUNT 是 GLSL 侧行表
    // 边界常量，至少为 1（material_data_index_rows 索引 0 仍有效）
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
    // FS 阶段注入 bindless 纹理行表：TextureLayerRowsData struct + buffer（named slot）。
    // 字段名 = TextureSlot 的 snake_case 名（GetTextureSlotName），顺序与枚举一致；
    // 内存布局与旧扁平 values[RANGE_SIZE] 逐字节相同，故 CPU 上传无需改动。
    // 行索引即 fragDataIndexID（P1-2e：TextureLayerID varying 已删除）。
    {
        // 仅当该材质确实注册了 mtl_texture_layer_rows（存在 MaterialTextureLayerTable
        // 描述符，即声明了纹理槽）时才注入 named-slot struct + buffer；否则跳过
        //（与旧 AppendIndexTableDecl 的静默跳过行为一致，无纹理槽材质 FS 不引用该 buffer）。
        const ShaderDescriptor *sd =
            descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name);
        if (sd && sd->set >= 0 && sd->binding >= 0)
        {
            fs_index_table_decls += "struct TextureLayerRowsData\n{\n";
            for (uint32_t i = 0;
                 i < static_cast<uint32_t>(TextureSlot::RANGE_SIZE); ++i)
            {
                fs_index_table_decls += "    uint ";
                fs_index_table_decls += GetTextureSlotName(static_cast<TextureSlot>(i));
                fs_index_table_decls += ";\n";
            }
            fs_index_table_decls += "};\n";
            fs_index_table_decls += "layout(set=" + std::to_string(sd->set)
                                  + ", binding=" + std::to_string(sd->binding)
                                  + ") readonly buffer TextureLayerRowsBuffer\n{\n"
                                  + "    TextureLayerRowsData data[];\n"
                                  + "} mtl_texture_layer_rows;\n";
        }
    }

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
        // B6: 单一 marker（#include SURFACE_FUNCTION_FILE）——"#include "surface/" 旧格式
        // 回退已删（全库 0 使用——ShaderLibrary 与回归门均无）
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        const auto pos = glsl.find(marker);
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos) + inject + "\n" + glsl.substr(pos);
    };

    std::string vs_final = InsertAfterVersionLine(vs_glsl, binding_preamble + vs_index_table_decls);

    // 注入顺序（InsertAfterVersionLine 后注入者位于 version 后更前）：
    // 模块代码可能引用 SSBO 类型、MTL_DATA 宏、索引行表与采样器宏
    // （unlit_source 的 EmissiveSurfaceData / MTL_DATA；bindless_textures 的
    // mtl_texture_layer_rows / TrilinearSampler）——全部注入统一放最后
    // （version 后最前，先于模块代码）
    std::string fs_final = fs_glsl;
    const std::string code_module_glsl = BuildCodeModuleGLSL(config.resource_manifest);
    if (!code_module_glsl.empty())
    {
        if (fs_glsl.find("#include SURFACE_FUNCTION_FILE") == std::string::npos)
            fs_final = InsertAfterVersionLine(fs_final, code_module_glsl);
        else
            fs_final = InsertBeforeSurfaceFunction(fs_final, code_module_glsl);
    }
    fs_final = InsertAfterVersionLine(
        fs_final,
        binding_preamble + sampler_macros + material_ssbo_decls
        + material_slot_macros + fs_index_table_decls);

    ShaderCreateInfoVertex   *vert = ctx->GetVertexShader();
    ShaderCreateInfo         *frag = ctx->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);

    // ─────────────────────────────────────────────────────────────
    // Step 6b: Build ShaderResourceSchema from descriptor entries.
    // When data_slot_decls is provided, material SSBO entries are
    // generated from it and merged with the canonical descriptor entries.
    // ─────────────────────────────────────────────────────────────

    ShaderResourceSchema shader_resource_schema;
    if (!BuildResourceSchemaFromContract(
            effective_descriptor_contract,
            shader_resource_schema))
        return FailAfterBuild("descriptor contract/layout build failed");

    std::vector<std::string> contract_diagnostics;
    if (!ValidateShaderResourceSchema(shader_resource_schema, contract_diagnostics))
    {
        for (const auto &diag : contract_diagnostics)
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial][ShaderResourceSchema] material=%s: %s\n",
                input.debug_name ? input.debug_name : "<unnamed>",
                diag.c_str());
        }
        return FailAfterBuild("ShaderResourceSchema validation failed");
    }

    if (config.material_definition)
    {
        const MaterialDefinition &material_definition = *config.material_definition;
        std::vector<std::string> capability_diagnostics;
        if (!ValidateDefinitionCapabilitySubset(
                material_definition, shader_resource_schema,
                capability_diagnostics, config.resource_manifest))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][DefinitionCapability] material=%s: %s\n",
                    input.debug_name ? input.debug_name : "<unnamed>",
                    diag.c_str());
            }
            return FailAfterBuild("Definition capability subset validation failed");
        }
    }

    ctx->SetShaderResourceSchema(shader_resource_schema);
    if (ctx->HasProgramLink())
    {
        ShaderProgramArtifactMetadata metadata{};
        if (!BuildShaderProgramArtifactMetadata(
                profile, *ctx, metadata))
            return FailAfterBuild(
                "failed to build ShaderProgram artifact metadata");
        ctx->SetProgramArtifactMetadata(metadata);
    }

    // ─────────────────────────────────────────────────────────────
    // Step 7: Compile directly → SPV
    // ─────────────────────────────────────────────────────────────

    if (config.defer_finalize)
        return ctx;

    if (!FinalizeShaderBuildContext(ctx))
        return FailAfterBuild(
            "FinalizeShaderBuildContext() failed");

    return ctx;
}

}  // namespace hgl::graph::mtl
