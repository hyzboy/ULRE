/// MaterialShaderCompiler.cpp — canonical material input compiler
///
/// 流程（BDA 终态，描述符分配器已退役）：
///   1. 求解层构建契约/schema（数据槽信号经 requires_runtime_data_rows 直判）
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
#include "compile/MaterialShaderEmitter.h"
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {
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

#ifdef _DEBUG
    // ULRE_DUMP_GLSL=1: dump final GLSL of every material build (mesh+fragment).
    // 输出目录由 DumpShaderGenGLSL 解析（<ShaderCacheRoot>/glsldump，不绑定机器路径）。
    if (const ShaderCreateInfo *mesh_si =
            build_spec->GetStageShader(ShaderStage::Mesh))
        DumpShaderGenGLSL("mesh", mesh_si->GetFinalGLSL());

    if (const ShaderCreateInfo *frag_si =
            build_spec->GetStageShader(ShaderStage::Fragment))
        DumpShaderGenGLSL("fs", frag_si->GetFinalGLSL());
#endif
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
                fragment->GetSPVSize()))
            return false;

        if (!artifact_store->SaveProgramMetadata(
                link,
                build_spec->GetProgramArtifactMetadata()))
            return false;

        // 生成的最终 GLSL 源随 SPV 一起落盘：与 stage SPV 缓存文件
        // 同样的主文件名（同目录，扩展名按 stage 定——.mesh/.frag），
        // 便于日后对照分析。Best-effort：失败只记日志，不阻断 shader 缓存。
        const std::string &mesh_glsl = vertex->GetFinalGLSL();
        if (!mesh_glsl.empty())
            artifact_store->SaveStageGLSL(link.mesh_stage,
                                          mesh_glsl.data(),
                                          mesh_glsl.size());
        const std::string &fragment_glsl = fragment->GetFinalGLSL();
        if (!fragment_glsl.empty())
            artifact_store->SaveStageGLSL(link.fragment_stage,
                                          fragment_glsl.data(),
                                          fragment_glsl.size());
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

// ── 能力子集授权规则表（原 10 分支 switch 表驱动化）────────────────────────
// 有条件内置资源的 definition 侧授权谓词，与资源目录（DescriptorResourceCatalog）
// 平行：目录行 engine_builtin=false 且有 definition 侧规则的语义在此登记，
// 交叉覆盖由下方 static_assert 保证。provider manifest 不再授权材质 payload。
using DefinitionCapabilityRule =
    bool (*)(const MaterialDefinition &definition,
             const ShaderResourceSlot &req) noexcept;

bool RuleUBORequirement(
    const MaterialDefinition &definition,
    const ShaderResourceSlot &req) noexcept
{
    return HasUBORequirement(definition, req.semantic);
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
    std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    for (const auto &req : layout.resources)
    {
        const DescriptorResourceCatalogEntry *cat =
            FindResourceCatalogEntry(req.semantic);

        // 授权两层：① 无条件内置（目录 engine_builtin）→
        // ② definition 侧规则（能力规则表）。
        bool allowed = cat && cat->engine_builtin;

        if (!allowed && cat)
        {
            const DefinitionCapabilityRule rule =
                FindDefinitionCapabilityRule(req.semantic);
            if (rule)
                allowed = rule(definition, req);
            // 未登记规则 = 无 definition 侧授权（Unknown、MaterialTexture/Sampler
            // 等 bindless 通道）——保持 false
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
// CompileMaterial — Compositor 模板完整 GLSL → ShaderBuildContext
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// CompileMaterial 内部辅助实现
//
// CompileMaterial 的 7 步流水线被拆分为多个静态辅助函数，
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
            "[CompileMaterial] material=%s failed: %s\n",
            c.input && c.input->debug_name ? c.input->debug_name : "<unnamed>",
            c.last_error.c_str());
    }
    delete c.ctx;
    c.ctx = nullptr;
    return nullptr;
}

static void CaptureProductionSnapshot(
    MaterialShaderDocumentCapture *document_capture,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const ShaderDocument &mesh_final_document,
    const ShaderDocument &fragment_final_document)
{
    if (!document_capture)
        return;

    const ShaderDocument mesh_source_snapshot = mesh_source_document;
    const ShaderDocument fragment_source_snapshot = fragment_source_document;
    const ShaderDocument mesh_final_snapshot = mesh_final_document;
    const ShaderDocument fragment_final_snapshot = fragment_final_document;

    document_capture->Clear();
    document_capture->mesh_source_document = mesh_source_snapshot;
    document_capture->fragment_document = fragment_source_snapshot;
    document_capture->mesh_final_document = mesh_final_snapshot;
    document_capture->fragment_final_document = fragment_final_snapshot;
}

// ── Step 1: Config（primitive 校验留在主函数）────────────────────────────────
static bool PrepareBaseDescriptorContract(
    const MaterialShaderCompilerInput &input,
    const MaterialCompileConfig &config,
    uint32_t &out_shader_stage_bits,
    DescriptorContract &out_base_contract,
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

    // A6-2a：契约不再含 L2W 条目（Push 已删）——L2W 需求按模板类别静态判定，
    // mesh 为唯一顶点路径且 l2w_ssbo 恒注入；无 L2W 材质不消费地址，
    // 运行时 push nullptr 安全（has_l2w_matrix ctx 链已随 A6-2b 删除）。
    return true;
}

// ── Step 2: 创建 ShaderBuildContext ─────────────────────────────────────────
static bool CreateBuildContext(
    const MaterialCompileConfig &config,
    const uint32_t shader_stage_bits,
    CompileContext &c)
{
    c.ctx = new ShaderBuildContext(config.primitive_type, shader_stage_bits);
    if (config.program_link)
        c.ctx->SetProgramLink(*config.program_link);
    c.ctx->SetArtifactStore(config.artifact_store);
    return true;
}

// ── Step 3a: 解析有效材质私有数据 SSBO 类型（编译配置单一声明）──────────────
// 一个材质只有一个私有数据 SSBO（MaterialPrivateData，名字固定
// DefaultMaterialPrivateDataName），类型由 MaterialDefinition/CompileConfig
// 传入，不再从 provider manifest 推导。
static bool ResolveEffectiveMaterialPrivateData(
    const MaterialCompileConfig &config,
    CompileContext &c,
    GlobalSSBOType &out_material_private_data)
{
    (void)c;
    out_material_private_data = config.material_private_data;

    // Material payloads are tracked by the material-specific enum; the generic
    // SSBOType namespace is reserved for non-material resources only.
    return IsGlobalSSBOType(out_material_private_data);
}

// ── Step 3b: 有效契约 ────────────────────────────────────────────────────────
// A6-2b-b2：数据槽行表不再补录进契约——改为编译期直判信号
// schema.requires_runtime_data_rows（Step 6 设置，条件同 emit_data_index_id），
// 渲染侧建表/绑定表判定统一读该标志。契约恒 Scene UBO 条目。
static bool BuildEffectiveDescriptorEntries(
    const DescriptorContract &base_contract,
    const GlobalSSBOType material_private_data,
    CompileContext &c,
    DescriptorContract &out_effective_contract)
{
    (void)material_private_data;

    if (!BuildEffectiveDescriptorContract(
            base_contract,
            material_private_data,
            out_effective_contract))
        return c.Fail("invalid effective material descriptor contract");

    return true;
}

// ── 已删除：Step 3c canonical 描述符注册 ─────────────────────────────────────
// 原 RegisterCanonicalDescriptors() 遍历契约条目查资源目录后什么都不做——
// Scene UBO 已全局化（P1），per-material 分配器随 BDA 化退场，无条目触发注册。
// 该函数与其唯一消费的 SerializedDescriptorEntry[] 副本一并移除。

// ── Step 5a: set/binding 宏 ──────────────────────────────────────────────────
// 固定 ABI 的 set/binding 宏（L2W/MESH_DRAW_PARAMS/VIEWPORT/CAMERA/SKY/COLOR_PALETTE
// 及顶点系列）不再由编译器注入：descriptor_macros.glsl 为生成物
//（DescriptorMacroGen，数值真源 DescriptorSetTypeDef.h 的绑定枚举），模板与模块
// #include 后直接使用默认值，单一真源。
//
// 行表（material_data_addresses / l2w_index）无 set/binding 概念：
// BDA 化后行表经 pc_root + buffer_reference 寻址，声明由 index table 生成器恒发射。

// ── Step 6: ShaderResourceSchema 构建与校验 ──────────────────────────────────
static bool BuildAndValidateResourceSchema(
    const DescriptorContract &effective_descriptor_contract,
    const MaterialCompileConfig &config,
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
                "[CompileMaterial][ShaderResourceSchema] material=%s: %s\n",
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
                capability_diagnostics))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileMaterial][DefinitionCapability] material=%s: %s\n",
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

ShaderBuildContext *CompileMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const MaterialCompileConfig &config)
{
    return CompileMaterial(
        profile, input, mesh_source_document, fragment_source_document, config, nullptr);
}

ShaderBuildContext *CompileMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const MaterialCompileConfig &config,
    MaterialShaderDocumentCapture *document_capture)
{
    // ctx 尚未创建，此处 FailCompile 只负责统一报告（delete nullptr 安全）
    CompileContext c{&input};

    if (mesh_source_document.GetBlockCount() == 0
     || fragment_source_document.GetBlockCount() == 0)
    {
        c.Fail("source document is empty");
        return FailCompile(c);
    }

    // ── Step 1: Config ────────────────────────────────────────────
    const PrimitiveType primitive_type = config.primitive_type;
    if (input.primitive_type != primitive_type)
    {
        c.Fail("primitive_type mismatch (input="
               + std::to_string(int(input.primitive_type))
               + ", config="
               + std::to_string(int(primitive_type)) + ")");
        return FailCompile(c);
    }

    if (config.resource_manifest
     && config.resource_manifest->IsValid()
     && config.material_definition)
    {
        if (!ValidateShaderCodeResourceManifestTextureReferences(
                *config.resource_manifest,
                *config.material_definition))
        {
            c.Fail(
                "Resource manifest references a texture absent from its material definition");
            return FailCompile(c);
        }
    }

    DescriptorContract base_descriptor_contract{};
    uint32_t shader_stage_bits = 0;
    if (!PrepareBaseDescriptorContract(input, config,
                                       shader_stage_bits,
                                       base_descriptor_contract,
                                       c))
        return FailCompile(c);

    // ── Step 2: Create ShaderBuildContext ─────────────────────────
    if (!CreateBuildContext(config, shader_stage_bits, c))
        return FailCompile(c);

    ShaderBuildContext *ctx = c.ctx;

    // ── Step 3: Add Descriptors from SerializedDescriptorEntry[] ──
    // MaterialDefinition/CompileConfig supplies the material payload type;
    // provider metadata no longer contributes descriptor declarations.
    GlobalSSBOType effective_material_private_data = GlobalSSBOType::PBRSurface;
    if (!ResolveEffectiveMaterialPrivateData(config, c, effective_material_private_data))
        return FailCompile(c);

    DescriptorContract effective_descriptor_contract{};
    if (!BuildEffectiveDescriptorEntries(
            base_descriptor_contract, effective_material_private_data,
            c,
            effective_descriptor_contract))
        return FailCompile(c);

    // ── Step 5: Complete both stages through ShaderDocument ───────
    ShaderDocument local_mesh_final_document;
    ShaderDocument local_fragment_final_document;
    ShaderDocument *mesh_final_document = &local_mesh_final_document;
    ShaderDocument *fragment_final_document = &local_fragment_final_document;
    if (document_capture)
    {
        mesh_final_document = &document_capture->mesh_final_document;
        fragment_final_document = &document_capture->fragment_final_document;
    }
    ShaderDocumentDiagnostics document_diagnostics;
    if (!BuildMaterialStageDocument(
            mesh_source_document,
            ShaderStage::Mesh,
            input.debug_name,
            config,
            effective_material_private_data,
            *mesh_final_document,
            document_diagnostics))
    {
        c.Fail("Material stage document build failed");
        return FailCompile(c);
    }
    if (!BuildMaterialStageDocument(
            fragment_source_document,
            ShaderStage::Fragment,
            input.debug_name,
            config,
            effective_material_private_data,
            *fragment_final_document,
            document_diagnostics))
    {
        c.Fail("Material stage document build failed");
        return FailCompile(c);
    }

    if (document_capture)
    {
        CaptureProductionSnapshot(
            document_capture,
            mesh_source_document,
            fragment_source_document,
            *mesh_final_document,
            *fragment_final_document);
    }

    AnsiString mesh_final_glsl;
    AnsiString fragment_final_glsl;
    if (!mesh_final_document->Serialize(mesh_final_glsl, document_diagnostics)
     || !fragment_final_document->Serialize(fragment_final_glsl, document_diagnostics))
    {
        c.Fail("Material stage document serialization failed");
        return FailCompile(c);
    }

    if (document_capture)
    {
        CaptureProductionSnapshot(
            document_capture,
            mesh_source_document,
            fragment_source_document,
            *mesh_final_document,
            *fragment_final_document);
    }

    ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
    ShaderCreateInfo *frag = ctx->GetStageShader(ShaderStage::Fragment);
    if (mesh)
        mesh->SetFinalGLSL(std::string(mesh_final_glsl.c_str(), mesh_final_glsl.Length()));
    if (frag)
        frag->SetFinalGLSL(std::string(fragment_final_glsl.c_str(), fragment_final_glsl.Length()));


    // ── Step 6: Build ShaderResourceSchema from descriptor entries. ──
    // Material payload declarations are generated from the effective material type;
    // they are not read from provider resource metadata.
    ShaderResourceSchema shader_resource_schema;
    if (!BuildAndValidateResourceSchema(effective_descriptor_contract, config, c,
                                        shader_resource_schema))
        return FailCompile(c);

    // Runtime address rows are required by payload data or an active
    // texture-reference consumer. Depth variants may retain the definition's
    // declarations while omitting every provider that samples them.
    const bool has_active_texture_reference_consumer =
        config.material_definition
        && !config.material_definition->texture_declarations.empty()
        && (!config.resource_manifest
         || config.resource_manifest->texture_reference_count != 0);
    const bool has_material_ssbo_payload =
        IsGlobalSSBOType(effective_material_private_data)
        && (!config.material_definition
         || config.material_definition->vertex_varying.emit_data_index_id);
    shader_resource_schema.requires_runtime_data_rows =
        has_material_ssbo_payload
        || (config.material_definition
         && (config.material_definition->vertex_varying.emit_data_index_id
          || has_active_texture_reference_consumer));

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
