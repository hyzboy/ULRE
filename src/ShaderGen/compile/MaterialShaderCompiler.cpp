/// MaterialShaderCompiler.cpp — canonical material input compiler
///
/// 流程：
///   1. 从 SerializedDescriptorEntry[] 构建 DescriptorSetLayoutAllocator（描述符布局）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include "builder/DescriptorBuilderCommon.h"
#include "compile/MaterialShaderEmitter.h"
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
            // mesh shader 材质：顶点处理 stage 是 Mesh
            ShaderCreateInfo *vertex =
                build_spec->GetStageShader(ShaderStage::Mesh);
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
        // mesh shader 材质：顶点处理 stage 是 Mesh
        const ShaderCreateInfo *vertex =
            build_spec->GetStageShader(ShaderStage::Mesh);
        const ShaderCreateInfo *fragment =
            build_spec->GetStageShader(ShaderStage::Fragment);
        if (!vertex || !fragment
         || !artifact_store->SaveStageSPV(
                link.mesh_stage,
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

static bool HasDescriptorSemantic(
    const DescriptorContract &contract,
    const DescriptorSemantic semantic)
{
    for (const SerializedDescriptorEntry &entry : contract)
    {
        if (entry.semantic == semantic)
            return true;
    }

    return false;
}

static bool AddMaterialPrivateDataSlotDescriptor(ShaderBuildContext &ctx,
                                          const SSBOType material_private_data,
                                          const uint32_t material_private_data_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialSSBOLayout(material_private_data, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!ctx.AddStruct(struct_name, glsl_codes))
        return false;

    return ctx.AddSSBOMaterialPrivateData(stage_bits, struct_name, DefaultMaterialPrivateDataSlotName, int(material_private_data_slot));
}

// ── 能力子集授权规则表（原 10 分支 switch 表驱动化）────────────────────────
// 有条件内置资源的 definition 侧授权谓词，与资源目录（DescriptorResourceCatalog）
// 平行：目录行 engine_builtin=false 且有 definition 侧规则的语义在此登记，
// 交叉覆盖由下方 static_assert 保证。manifest 侧回退（provider 元数据授权）
// 不在此表——它是独立的第二授权源，见 ValidateDefinitionCapabilitySubset。
using DefinitionCapabilityRule =
    bool (*)(const MaterialDefinition &definition,
             const ShaderResourceSlot &req) noexcept;

bool RuleUBORequirement(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &req) noexcept
{
    return HasUBORequirement(definition, req.semantic);
}

// L2W/L2WIndex：仅世界空间投影需要（OrthoViewport/ClipPassthrough 不经 L2W）
bool RuleWorldTransform(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &) noexcept
{
    return definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
        && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough;
}

bool RulePrivateDataSlot(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &req) noexcept
{
    // 单槽：req 指向的槽必须是 definition 声明的那个（slot 0），类型一致
    return req.material_private_data_slot == DefaultMaterialPrivateDataSlot
        && definition.material_private_data == req.ssbo_type;
}

bool RuleTextureLayerTable(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &) noexcept
{
    return !definition.texture_slot_decls.empty();
}

bool RulePrivateDataIndex(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &) noexcept
{
    return definition.material_private_data != SSBOType::UserDefined
        || definition.vertex_varying.emit_data_index_id;
}

struct DefinitionCapabilityRuleEntry
{
    DescriptorSemantic semantic;
    DefinitionCapabilityRule rule;
};

constexpr DefinitionCapabilityRuleEntry kDefinitionCapabilityRules[] =
{
    { DescriptorSemantic::CameraInfo,                &RuleUBORequirement },
    { DescriptorSemantic::SkyInfo,                   &RuleUBORequirement },
    { DescriptorSemantic::MaterialColorPalette,      &RuleUBORequirement },
    { DescriptorSemantic::LocalToWorld,              &RuleWorldTransform },
    { DescriptorSemantic::LocalToWorldIndex,         &RuleWorldTransform },
    { DescriptorSemantic::MaterialPrivateData,       &RulePrivateDataSlot },
    { DescriptorSemantic::MaterialTextureLayerTable, &RuleTextureLayerTable },
    { DescriptorSemantic::MaterialPrivateDataIndex,  &RulePrivateDataIndex },
};

constexpr DefinitionCapabilityRule FindDefinitionCapabilityRule(
    const DescriptorSemantic semantic) noexcept
{
    for (const auto &row : kDefinitionCapabilityRules)
        if (row.semantic == semantic)
            return row.rule;
    return nullptr;
}

// 交叉覆盖：规则表每一行必须是目录中 engine_builtin=false 的有条件行——
// 无条件内置行不需要规则；未登记目录的语义查表不可达。
constexpr bool CapabilityRulesMatchCatalog() noexcept
{
    for (const auto &row : kDefinitionCapabilityRules)
    {
        const DescriptorResourceCatalogEntry *cat =
            FindResourceCatalogEntry(row.semantic);
        if (!cat || cat->engine_builtin)
            return false;
    }
    return true;
}

static_assert(CapabilityRulesMatchCatalog(),
              "能力规则表行必须在资源目录中登记为有条件内置（engine_builtin=false）");

static bool ValidateDefinitionCapabilitySubset(
    const MaterialDefinition &definition,
    const ShaderResourceSchema &layout,
    std::vector<std::string> &diagnostics,
    const ShaderCodeResourceManifest *manifest)
{
    diagnostics.clear();

    for (const auto &req : layout.resources)
    {
        const DescriptorResourceCatalogEntry *cat =
            FindResourceCatalogEntry(req.semantic);

        // 授权三层：① 无条件内置（目录 engine_builtin）→
        // ② definition 侧规则（能力规则表）→ ③ manifest 侧回退（provider 元数据）。
        bool allowed = cat && cat->engine_builtin;

        if (!allowed && cat)
        {
            const DefinitionCapabilityRule rule =
                FindDefinitionCapabilityRule(req.semantic);
            if (rule)
                allowed = rule(definition, req);
            // 未登记规则 = 无 definition 侧授权（Unknown、MaterialTexture/Sampler
            // 等 bindless 通道）——保持 false 走 manifest 回退
        }

        if (!allowed && manifest && manifest->IsValid())
        {
            for (uint32 i = 0; i < manifest->ssbo_count && !allowed; ++i)
            {
                const auto &ssbo = manifest->ssbos[i];
                if (req.semantic == DescriptorSemantic::MaterialPrivateData
                 && req.material_private_data_slot == ssbo.material_private_data_slot
                 && req.ssbo_type == ssbo.ssbo_type
                 && descriptor_builder_common::CStrEqual(req.name.c_str(), ssbo.name))
                    allowed = true;
            }
            if (req.semantic == DescriptorSemantic::MaterialTextureLayerTable
             && manifest->texture_layer_count > 0)
                allowed = true;

            // The MaterialPrivateDataIndexRows table only exists to route instance IDs
            // to material data-slot SSBOs. If any material data-slot SSBO was
            // declared purely via provider manifest metadata (no matching
            // TOML [resources].ssbos entry), the index table requirement is
            // implied and must be accepted the same way.
            if (req.semantic == DescriptorSemantic::MaterialPrivateDataIndex
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

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial 内部辅助实现
//
// CompileCompositorMaterial 的 7 步流水线被拆分为多个静态辅助函数，
// 描述符注册 / 行表声明 / binding 宏注入改为配表驱动，主函数仅做编排。
// 所有失败路径统一走 CompileContext::Fail() + FailCompile()（打印并 delete ctx）。
// ═══════════════════════════════════════════════════════════════════════════

struct CompileContext
{
    const MaterialShaderCompilerInput *input = nullptr;
    ShaderBuildContext *ctx = nullptr;
    std::string last_error;

    bool Fail(const std::string &reason)
    {
        last_error = reason;
        return false;
    }
};

static ShaderBuildContext *FailCompile(CompileContext &c)
{
    if (!c.last_error.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            c.input && c.input->debug_name ? c.input->debug_name : "<unnamed>",
            c.last_error.c_str());
    }
    delete c.ctx;
    c.ctx = nullptr;
    return nullptr;
}

// ── Step 1: Config（primitive 校验留在主函数）────────────────────────────────
static bool PrepareBaseDescriptorContract(
    const MaterialShaderCompilerInput &input,
    const CompositorMaterialBuildConfig &config,
    uint32_t &out_shader_stage_bits,
    DescriptorContract &out_base_contract,
    bool &out_with_local_to_world,
    CompileContext &c)
{
    out_shader_stage_bits = config.shader_stage_flag_bits != 0
        ? config.shader_stage_flag_bits
        : uint32_t(ShaderStage::MeshFragment);

    if (config.descriptor_contract)
    {
        out_base_contract = *config.descriptor_contract;
    }
    else if (!BuildDescriptorContract(
                input.descriptor_entries,
                input.descriptor_entry_count,
                out_base_contract))
    {
        return c.Fail("BuildDescriptorContract failed");
    }

    out_with_local_to_world = HasDescriptorSemantic(
        out_base_contract, DescriptorSemantic::LocalToWorld);
    return true;
}

// ── Step 2: 创建 ShaderBuildContext ─────────────────────────────────────────
static bool CreateBuildContext(
    const CompositorMaterialBuildConfig &config,
    const uint32_t shader_stage_bits,
    const bool with_local_to_world,
    CompileContext &c)
{
    c.ctx = new ShaderBuildContext(config.primitive_type, shader_stage_bits, with_local_to_world);
    if (config.program_link)
        c.ctx->SetProgramLink(*config.program_link);
    c.ctx->SetArtifactStore(config.artifact_store);
    return true;
}

// ── Step 3a: 解析有效材质私有数据 SSBO 类型（definition 单槽 ⊕ provider manifest）────
// 单槽化：一个材质只有一个私有数据 SSBO（MaterialPrivateData，slot 0，
// 名字固定 DefaultMaterialPrivateDataSlotName）。definition 侧与 manifest 侧
// 均可选；双源并存时必须类型一致，否则冲突硬失败。
// 顶点数据 SSBO（Vertex*）走固定名路径（PerObject 集），不进入材质数据槽。
static bool ResolveEffectiveMaterialPrivateData(
    const CompositorMaterialBuildConfig &config,
    CompileContext &c,
    SSBOType &out_material_private_data)
{
    out_material_private_data = config.material_private_data;

    if (config.merge_resource_manifest_material_slots
     && config.resource_manifest
     && config.resource_manifest->IsValid())
    {
        for (uint32_t i = 0; i < config.resource_manifest->ssbo_count; ++i)
        {
            const auto &ssbo = config.resource_manifest->ssbos[i];

            if (ssbo.material_private_data_slot != DefaultMaterialPrivateDataSlot)
                continue;

            if (out_material_private_data == SSBOType::UserDefined)
                out_material_private_data = ssbo.ssbo_type;
            else if (out_material_private_data != ssbo.ssbo_type)
                return c.Fail("provider material data slot conflicts with definition");
        }
    }

    return true;
}

// ── Step 3b: 有效契约 → 固定序列化条目 ───────────────────────────────────────
static bool BuildEffectiveDescriptorEntries(
    const CompositorMaterialBuildConfig &config,
    const DescriptorContract &base_contract,
    const SSBOType material_private_data,
    const uint32_t material_ssbo_stage_bits,
    CompileContext &c,
    DescriptorContract &out_effective_contract,
    std::vector<SerializedDescriptorEntry> &out_entries,
    uint32_t &out_declared_slot_count)
{
    out_declared_slot_count =
        material_private_data != SSBOType::UserDefined ? 1u : 0u;

    if (!BuildEffectiveDescriptorContract(
            base_contract,
            material_private_data,
            material_ssbo_stage_bits,
            out_effective_contract))
        return c.Fail("invalid effective material descriptor contract");

    if (config.material_definition
     && !EnsureDescriptorContractVaryingResources(
            config.material_definition->vertex_varying,
            out_effective_contract))
        return c.Fail("failed to add varying descriptor contract resources");

    // C1-T2：entries 即规范化 SerializedDescriptorEntry[]（原
    // ConvertDescriptorContractToFixed 往返转换已删——直接取契约条目）
    out_entries = out_effective_contract;

    return true;
}

// ── Step 3c: canonical 描述符注册（目录表驱动）───────────────────────────────
// 唯一真源：inc/hgl/mtl/DescriptorResourceCatalog.h（语义→类别/集合/绑定/SBS）。
// 按类别三分支：SceneGlobal 全局化跳过；PerDraw/VertexGeometry 固定 ABI 注册；
// MaterialData per-material 动态（数据槽由 RegisterMaterialPrivateDataSlotDescriptors
// 单独处理，此处仅纹理层表/行表）。
static bool RegisterCanonicalDescriptors(
    ShaderBuildContext *ctx,
    const std::vector<SerializedDescriptorEntry> &descriptor_entries,
    const uint32_t declared_material_private_data_slot_count,
    uint32_t &io_material_ssbo_stage_bits,
    CompileContext &c)
{
    for (const SerializedDescriptorEntry &entry : descriptor_entries)
    {
        const uint32_t stage_bits = entry.stage_flags;

        // MaterialPrivateData：由单槽 effective material_private_data 注册，
        // 此处只累加其 stage bits（渲染侧固定为 SSBO 语义）。
        if (entry.semantic == DescriptorSemantic::MaterialPrivateData)
        {
            io_material_ssbo_stage_bits = stage_bits;
            continue;
        }

        const DescriptorResourceCatalogEntry *cat =
            FindResourceCatalogEntry(entry.semantic);
        if (!cat)
            continue;

        switch (cat->cls)
        {
        case ResourceCatalogClass::SceneGlobal:
            // Scene UBO 已全局化（P1/P1-2a），不再进入 per-material 分配器
            break;

        case ResourceCatalogClass::PerDraw:
            if (cat->semantic == DescriptorSemantic::LocalToWorld)
            {
                if (!ctx->SetLocalToWorld(stage_bits))
                    return c.Fail("failed to set LocalToWorld SSBO");
            }
            else if (cat->semantic == DescriptorSemantic::MaterialPrivateDataIndex)
            {
                if (!ctx->AddStruct(SBS_MaterialPrivateDataIndexRows.struct_name, ""))
                    return c.Fail("failed to add MaterialPrivateDataIndex struct");
                // P1-2c：行表迁至 PerObject 集，binding 由固定枚举确定（固定名路径）
                if (!ctx->AddSSBOMaterialPrivateDataIndex(stage_bits))
                    return c.Fail("failed to add MaterialPrivateDataIndex SSBO");
            }
            else
            {
                // LocalToWorldIndex / MeshDrawParams：SBS + 固定 binding 目录行
                if (!ctx->AddSSBOVertex(stage_bits, *cat->sbs, cat->binding))
                    return c.Fail(std::string("failed to add ") + cat->sbs->name + " SSBO");
            }
            break;

        case ResourceCatalogClass::VertexGeometry:
            if (cat->semantic == DescriptorSemantic::VertexIndex)
            {
                if (!ctx->AddSSBOVertexIndex(stage_bits))
                    return c.Fail("failed to add VertexIndex SSBO");
            }
            else
            {
                if (!ctx->AddSSBOVertex(stage_bits, *cat->sbs, cat->binding))
                    return c.Fail(std::string("failed to add ") + cat->sbs->name + " SSBO");
            }
            break;

        case ResourceCatalogClass::MaterialData:
            if (cat->semantic == DescriptorSemantic::MaterialTextureLayerTable)
            {
                if (!ctx->AddStruct(SBS_MaterialTextureLayerRows.struct_name, ""))
                    return c.Fail("failed to add MaterialTextureLayerRows struct");
                // 单槽化：材质至多一个数据槽，纹理层行表紧随其后（binding=槽数）
                if (!ctx->AddSSBOTextureLayer(stage_bits, int(declared_material_private_data_slot_count)))
                    return c.Fail("failed to add MaterialTextureLayerRows SSBO");
            }
            // MaterialTexture/MaterialSampler：bindless 通道，无 per-material 描述符
            break;
        }
    }

    return true;
}

// ── CharQuad text SSBOs: mesh shader declares these inline, register them into PerObject set layout ──
// Register the three CharQuad SSBOs at fixed bindings 14/15/16
// matching TEXT_CHARINFO_BINDING/TEXT_CHARSTYLE_BINDING/TEXT_CHARINSTANCE_BINDING
// in descriptor_macros.glsl
// 注意：结构体的 GLSL 声明真源是
//   结构体 GLSL 真源 = ShaderLibrary/vertex/s1_text_char_quad.glsl（T2.1 已归一，
//   由 MeshShaderModeCharQuad.h::EmitCharQuadSSBODeclarations include）；
//   CPU 侧布局 = inc/hgl/graph/font/TextCharSSBO.h（static_assert 校验）。
//   改结构布局必须改 .glsl + TextCharSSBO.h 两侧。
// 此处只注册 set layout，结构体代码由 mesh shader 生成器提供（pass empty codes）。
struct CharQuadSSBOReg
{
    const char *struct_name;
    const char *sbo_name;
    int binding;        // PerObjectBinding::TextChar* 枚举
};

static const CharQuadSSBOReg kCharQuadSSBOTable[] = {
    { "TextCharInfo",     "sbo_char_info",     int(PerObjectBinding::TextCharInfo) },
    { "CharStyleData",    "sbo_char_style",    int(PerObjectBinding::TextCharStyle) },
    { "CharInstanceData", "sbo_char_instance", int(PerObjectBinding::TextCharInstance) },
};

static bool RegisterCharQuadSSBOs(
    ShaderBuildContext *ctx,
    const uint32_t stage_bits,
    CompileContext &c)
{
    for (const CharQuadSSBOReg &reg : kCharQuadSSBOTable)
    {
        if (!ctx->AddStruct(reg.struct_name, ""))
            return c.Fail(std::string("failed to add ") + reg.struct_name + " struct");
        if (!ctx->AddSSBO(stage_bits, DescriptorSetType::PerObject,
                          reg.struct_name, reg.sbo_name, reg.binding))
            return c.Fail(std::string("failed to add ") + reg.sbo_name + " SSBO");
    }

    return true;
}

// ── Step 3d: 材质数据槽描述符（单槽：固定 slot 0 / DefaultMaterialPrivateDataSlotName）──
static bool RegisterMaterialPrivateDataSlotDescriptors(
    ShaderBuildContext *ctx,
    const SSBOType material_private_data,
    const uint32_t material_ssbo_stage_bits,
    CompileContext &c)
{
    if (material_private_data == SSBOType::UserDefined)
        return true;

    if (!AddMaterialPrivateDataSlotDescriptor(*ctx, material_private_data, DefaultMaterialPrivateDataSlot, material_ssbo_stage_bits))
        return c.Fail("failed to add declared material ssbo slot descriptor");

    return true;
}

// ── Step 5a: set/binding 宏 ──────────────────────────────────────────────────
// 固定 ABI 的 set/binding 宏（L2W/MESH_DRAW_PARAMS/VIEWPORT/CAMERA/SKY/COLOR_PALETTE
// 及顶点系列）不再由编译器注入：descriptor_macros.glsl 为生成物
//（DescriptorMacroGen，数值真源 DescriptorSetTypeDef.h 的绑定枚举），模板与模块
// #include 后直接使用默认值，单一真源。
//
// 行表绑定（material_private_data_index_rows / mtl_texture_layer_rows / l2w_index）不在此
// 注入 set/binding 宏：声明由 index table 生成逻辑依据 descriptor_info 直接以
// layout(set=.., binding=..) 写出（统一声明生成，不再写死在 .glsl）。

// ── Step 6: ShaderResourceSchema 构建与校验 ──────────────────────────────────
static bool BuildAndValidateResourceSchema(
    const DescriptorContract &effective_descriptor_contract,
    const CompositorMaterialBuildConfig &config,
    CompileContext &c,
    ShaderResourceSchema &out_schema)
{
    if (!BuildResourceSchemaFromContract(
            effective_descriptor_contract,
            out_schema))
        return c.Fail("descriptor contract/layout build failed");

    std::vector<std::string> contract_diagnostics;
    if (!ValidateShaderResourceSchema(out_schema, contract_diagnostics))
    {
        for (const auto &diag : contract_diagnostics)
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial][ShaderResourceSchema] material=%s: %s\n",
                c.input->debug_name ? c.input->debug_name : "<unnamed>",
                diag.c_str());
        }
        return c.Fail("ShaderResourceSchema validation failed");
    }

    if (config.material_definition)
    {
        std::vector<std::string> capability_diagnostics;
        if (!ValidateDefinitionCapabilitySubset(
                *config.material_definition,
                out_schema,
                capability_diagnostics,
                config.resource_manifest))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][DefinitionCapability] material=%s: %s\n",
                    c.input->debug_name ? c.input->debug_name : "<unnamed>",
                    diag.c_str());
            }
            return c.Fail("Definition capability subset validation failed");
        }
    }

    return true;
}

// ── Step 6b: ShaderProgram artifact metadata ─────────────────────────────────
static bool BuildArtifactMetadata(
    const contract::PhysicalDeviceProfileLite *profile,
    ShaderBuildContext *ctx,
    CompileContext &c)
{
    if (!ctx->HasProgramLink())
        return true;

    ShaderProgramArtifactMetadata metadata{};
    if (!BuildShaderProgramArtifactMetadata(profile, *ctx, metadata))
        return c.Fail("failed to build ShaderProgram artifact metadata");
    ctx->SetProgramArtifactMetadata(metadata);
    return true;
}

ShaderBuildContext *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const std::string &         ms_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config)
{
    return CompileCompositorMaterial(
        profile, input, ms_glsl, fs_glsl, config, nullptr);
}

ShaderBuildContext *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const std::string &ms_glsl,
    const std::string &fs_glsl,
    const CompositorMaterialBuildConfig &config,
    MaterialShaderDocumentCapture *document_capture)
{
    if (ms_glsl.empty() || fs_glsl.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s: ms_glsl or fs_glsl is empty\n",
            input.debug_name ? input.debug_name : "<unnamed>");
        return nullptr;
    }

    // ── Step 1: Config ────────────────────────────────────────────
    const PrimitiveType primitive_type = config.primitive_type;
    if (input.primitive_type != primitive_type)
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s: primitive_type mismatch "
            "(input=%d, config=%d)\n",
            input.debug_name ? input.debug_name : "<unnamed>",
            int(input.primitive_type), int(primitive_type));
        return nullptr;
    }

    CompileContext c{&input};

    DescriptorContract base_descriptor_contract{};
    uint32_t shader_stage_bits = 0;
    bool with_local_to_world = false;
    if (!PrepareBaseDescriptorContract(input, config,
                                       shader_stage_bits,
                                       base_descriptor_contract,
                                       with_local_to_world,
                                       c))
        return FailCompile(c);

    // ── Step 2: Create ShaderBuildContext ─────────────────────────
    if (!CreateBuildContext(config, shader_stage_bits, with_local_to_world, c))
        return FailCompile(c);

    ShaderBuildContext *ctx = c.ctx;

    uint32_t material_ssbo_stage_bits = uint32_t(ShaderStage::Fragment);
    if (config.merge_resource_manifest_material_slots
     && config.resource_manifest
     && config.resource_manifest->IsValid())
    {
        for (uint32_t i = 0; i < config.resource_manifest->ssbo_count; ++i)
            material_ssbo_stage_bits |= config.resource_manifest->ssbos[i].stage_flags;
    }

    // ── Step 3: Add Descriptors from SerializedDescriptorEntry[] ──
    // Provider metadata contributes the material SSBO to the same canonical
    // declaration as the material definition (单槽 ⊕ 单源冲突检测).
    SSBOType effective_material_private_data = SSBOType::UserDefined;
    if (!ResolveEffectiveMaterialPrivateData(config, c, effective_material_private_data))
        return FailCompile(c);

    DescriptorContract effective_descriptor_contract{};
    std::vector<SerializedDescriptorEntry> descriptor_entries;
    uint32_t declared_material_private_data_slot_count = 0;
    if (!BuildEffectiveDescriptorEntries(
            config, base_descriptor_contract, effective_material_private_data,
            material_ssbo_stage_bits, c,
            effective_descriptor_contract, descriptor_entries,
            declared_material_private_data_slot_count))
        return FailCompile(c);

    if (!RegisterCanonicalDescriptors(ctx, descriptor_entries,
                                      declared_material_private_data_slot_count,
                                      material_ssbo_stage_bits, c))
        return FailCompile(c);

    // CharQuad text SSBOs: mesh shader declares these inline, register them into PerObject set layout
    if (config.material_definition
     && IsCharQuadMode(config.material_definition->mesh_shader_mode))
    {
        if (!RegisterCharQuadSSBOs(ctx, shader_stage_bits, c))
            return FailCompile(c);
    }

    if (!RegisterMaterialPrivateDataSlotDescriptors(
            ctx, effective_material_private_data, material_ssbo_stage_bits, c))
        return FailCompile(c);


    // ── Step 5: Set complete GLSL (bypass ProcXXX pipeline) ───────
    const DescriptorSetLayoutAllocator &descriptor_info = ctx->GetDescriptorAllocator();

    if (document_capture)
    {
        ShaderDocumentDiagnostics document_diagnostics;
        const AnsiString mesh_stage_glsl(
            ms_glsl.c_str(), int(ms_glsl.size()));
        const AnsiString fragment_stage_glsl(
            fs_glsl.c_str(), int(fs_glsl.size()));
        if (!BuildMaterialStageDocument(
                mesh_stage_glsl,
                ShaderStage::Mesh,
                input.debug_name,
                config,
                descriptor_info,
                effective_material_private_data,
                document_capture->mesh_final_document,
                document_diagnostics)
         || !BuildMaterialStageDocument(
                fragment_stage_glsl,
                ShaderStage::Fragment,
                input.debug_name,
                config,
                descriptor_info,
                effective_material_private_data,
                document_capture->fragment_final_document,
                document_diagnostics))
        {
            c.Fail("Material stage document build failed");
            return FailCompile(c);
        }
    }

    // Keep the established stage injection path as the compilation boundary
    // while Document captures are validated by the production compare gate.
    // The Document serializer's output is checked above; this avoids changing
    // artifact generation until its full release-path equivalence is proven.
    std::string material_ssbo_decls;
    std::string material_slot_macros;
    std::string emit_error;
    if (!BuildMaterialSSBODeclarations(
            descriptor_info,
            effective_material_private_data,
            material_ssbo_decls,
            material_slot_macros,
            emit_error))
    {
        c.Fail(emit_error);
        return FailCompile(c);
    }

    std::string sampler_macros;
    if (config.material_definition)
        sampler_macros =
            BuildSamplerMacros(config.material_definition->sampler_names);
    const std::string compile_define_macros = BuildCompileDefineMacros(config);
    const std::string mesh_index_table_decls =
        BuildMeshIndexTableDecls(descriptor_info);
    const std::string fs_index_table_decls =
        BuildFSIndexTableDecls(descriptor_info);
    const std::string fs_inject =
        "#extension GL_EXT_mesh_shader : require\n"
        + compile_define_macros + sampler_macros
        + material_ssbo_decls + material_slot_macros
        + fs_index_table_decls;
    AssembleFinalGLSL(
        ctx,
        ms_glsl,
        fs_glsl,
        mesh_index_table_decls,
        fs_inject);

    // ── Step 6: Build ShaderResourceSchema from descriptor entries. ──
    // When material_private_data is declared (definition or provider manifest), the
    // material SSBO entry is generated from it and merged with the canonical entries.
    ShaderResourceSchema shader_resource_schema;
    if (!BuildAndValidateResourceSchema(effective_descriptor_contract, config, c,
                                        shader_resource_schema))
        return FailCompile(c);

    ctx->SetShaderResourceSchema(shader_resource_schema);

    // ── 结构快照观察字段（不参与 shader 生成语义，仅 ShaderStructureDump 用）──
    // 把求解层产出的模块列表与有效 varying 存到 ctx，回归门/快照无需持有 plan。
    if (config.resource_manifest && config.resource_manifest->IsValid())
    {
        std::vector<std::string> module_names;
        module_names.reserve(config.resource_manifest->code_module_count);
        for (uint32_t i = 0; i < config.resource_manifest->code_module_count; ++i)
        {
            const char *name = config.resource_manifest->code_module_names[i];
            if (name && name[0])
                module_names.emplace_back(name);
        }
        ctx->SetResolvedModules(std::move(module_names));
    }
    if (config.material_definition)
        ctx->SetEffectiveVarying(config.material_definition->vertex_varying);

    if (!BuildArtifactMetadata(profile, ctx, c))
        return FailCompile(c);

    // ── Step 7: Compile directly → SPV ────────────────────────────
    if (config.defer_finalize)
        return ctx;

    if (!FinalizeShaderBuildContext(ctx))
    {
        c.Fail("FinalizeShaderBuildContext() failed");
        return FailCompile(c);
    }

    return ctx;
}

}  // namespace hgl::graph::mtl
