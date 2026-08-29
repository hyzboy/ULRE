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
#include <hgl/mtl/ShaderCreateInfo.h>
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

static bool AddMaterialPrivateDataSlotDescriptor(ShaderBuildContext &ctx,
                                          const MaterialPrivateDataSlotDeclaration &decl,
                                          const uint32_t material_private_data_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialSSBOLayout(decl.ssbo_type, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!ctx.AddStruct(struct_name, glsl_codes))
        return false;

    return ctx.AddSSBOMaterialPrivateData(stage_bits, struct_name, decl.name, int(material_private_data_slot));
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
            // Viewport 是场景级 UBO（Scene set binding=2，切换 FBO 必绑）——恒允许，
            // 不需要材质 TOML 显式声明（材质声明了也只是冗余）。
            allowed = true;
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
        case DescriptorSemantic::LocalToWorldIndex:
            allowed = definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
                   && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough;
            break;

        case DescriptorSemantic::MaterialPrivateData:
            allowed = req.material_private_data_slot < definition.material_private_data_slot_decls.size();
            if (allowed)
                allowed = definition.material_private_data_slot_decls[req.material_private_data_slot].ssbo_type == req.ssbo_type;
            break;

        case DescriptorSemantic::MaterialTextureLayerTable:
            allowed = !definition.texture_slot_decls.empty();
            break;
        case DescriptorSemantic::MaterialPrivateDataIndex:
            allowed = !definition.material_private_data_slot_decls.empty()
                   || definition.vertex_varying.emit_data_index_id;
            break;

        // 顶点数据 SSBO（MeshShader 方向）：顶点 SSBO 语义无条件允许（顶点输入统一为 SSBO）
        case DescriptorSemantic::VertexPosition:
        case DescriptorSemantic::VertexUV:
        case DescriptorSemantic::VertexNTB:
        case DescriptorSemantic::VertexColor:
        case DescriptorSemantic::VertexLuminance:
        case DescriptorSemantic::VertexTransformID:
        case DescriptorSemantic::VertexSize:
        case DescriptorSemantic::VertexIndex:
        // mesh per-draw 参数表（IndirectMeshDraw）：引擎级必备，同顶点 SSBO 无条件允许
        case DescriptorSemantic::MeshDrawParams:
            allowed = true;
            break;

        case DescriptorSemantic::Unknown:
            allowed = false;
            break;
        }

        if (!allowed && manifest && manifest->IsValid())
        {
            for (uint32 i = 0; i < manifest->ssbo_count && !allowed; ++i)
            {
                const auto &ssbo = manifest->ssbos[i];
                if (req.semantic == DescriptorSemantic::MaterialPrivateData
                 && req.material_private_data_slot == ssbo.material_private_data_slot
                 && req.ssbo_type == ssbo.ssbo_type
                 && CStrEq(req.name.c_str(), ssbo.name))
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
        // 原实现此处静默失败（ctx 尚未创建，不打印、不 delete），保持行为一致。
        return false;
    }

    out_with_local_to_world = HasDescriptorSemantic(
        out_base_contract, DescriptorSemantic::LocalToWorld);
    return true;
}

// ── Step 2: 创建 ShaderBuildContext ─────────────────────────────────────────
static bool CreateBuildContext(
    const contract::PhysicalDeviceProfileLite *profile,
    const CompositorMaterialBuildConfig &config,
    const uint32_t shader_stage_bits,
    const bool with_local_to_world,
    CompileContext &c)
{
    c.ctx = new ShaderBuildContext(config.primitive_type, shader_stage_bits, with_local_to_world);
    if (profile)
        c.ctx->SetDevice(profile);
    if (config.program_link)
        c.ctx->SetProgramLink(*config.program_link);
    c.ctx->SetArtifactStore(config.artifact_store);
    return true;
}

// ── Step 3a: 合并 provider manifest 与 material_private_data_slot_decls 的材质数据槽声明 ─────
// 单槽化：一个材质只有一个私有数据 SSBO（MaterialPrivateData，slot 0）。
// 顶点数据 SSBO（Vertex*）走固定名路径（PerObject 集），不进入材质数据槽。
static bool MergeMaterialPrivateDataSlotDeclarations(
    const CompositorMaterialBuildConfig &config,
    CompileContext &c,
    std::vector<MaterialPrivateDataSlotDeclaration> &out_effective_decls)
{
    if (config.material_private_data_slot_decls)
    {
        if (config.material_private_data_slot_decls->size() > MaxMaterialPrivateDataSlotsPerMaterial)
            return c.Fail("a material may declare at most one MaterialPrivateData slot");
        out_effective_decls = *config.material_private_data_slot_decls;
    }

    if (config.merge_resource_manifest_material_slots
     && config.resource_manifest
     && config.resource_manifest->IsValid())
    {
        for (uint32_t i = 0; i < config.resource_manifest->ssbo_count; ++i)
        {
            const auto &ssbo = config.resource_manifest->ssbos[i];

            if (ssbo.material_private_data_slot != DefaultMaterialPrivateDataSlot)
                continue;

            if (out_effective_decls.empty())
            {
                MaterialPrivateDataSlotDeclaration decl;
                decl.name = ssbo.name;
                decl.ssbo_type = ssbo.ssbo_type;
                out_effective_decls.push_back(decl);
            }
            else
            {
                const auto &decl = out_effective_decls[0];
                if (decl.name != ssbo.name || decl.ssbo_type != ssbo.ssbo_type)
                    return c.Fail("provider material data slot conflicts with definition");
            }
        }
    }

    return true;
}

// ── Step 3b: 有效契约 → 固定序列化条目 + 槽位名校验 ─────────────────────────
static bool BuildEffectiveDescriptorEntries(
    const CompositorMaterialBuildConfig &config,
    const DescriptorContract &base_contract,
    const std::vector<MaterialPrivateDataSlotDeclaration> *material_private_data_slot_decls,
    const uint32_t material_ssbo_stage_bits,
    CompileContext &c,
    DescriptorContract &out_effective_contract,
    std::vector<SerializedDescriptorEntry> &out_entries,
    uint32_t &out_declared_slot_count)
{
    const bool use_slot_decls = material_private_data_slot_decls != nullptr;
    out_declared_slot_count = use_slot_decls
        ? static_cast<uint32_t>(material_private_data_slot_decls->size()) : 0u;

    if (!BuildEffectiveDescriptorContract(
            base_contract,
            material_private_data_slot_decls,
            material_ssbo_stage_bits,
            out_effective_contract))
        return c.Fail("invalid effective material descriptor contract");

    if (config.material_definition
     && !EnsureDescriptorContractVaryingResources(
            config.material_definition->vertex_varying,
            out_effective_contract))
        return c.Fail("failed to add varying descriptor contract resources");

    if (!ConvertDescriptorContractToFixed(
            out_effective_contract, out_entries))
        return c.Fail("failed to adapt material descriptor contract");

    if (use_slot_decls)
    {
        // 单槽化：一个材质至多一个 MaterialPrivateData 槽，名称固定。
        if (out_declared_slot_count > MaxMaterialPrivateDataSlotsPerMaterial)
            return c.Fail("a material may declare at most one MaterialPrivateData slot");

        for (uint32_t i = 0; i < out_declared_slot_count; ++i)
        {
            const auto &decl = (*material_private_data_slot_decls)[i];
            if (!IsValidMaterialPrivateDataSlotName(decl.name))
                return c.Fail("invalid material data slot GLSL name");

            if (decl.name != DefaultMaterialPrivateDataSlotName)
                return c.Fail("material data slot GLSL name must be MaterialPrivateData");
        }
    }

    return true;
}

// ── Step 3c: canonical 描述符注册（配表）─────────────────────────────────────
enum class RegisterOp : int
{
    None,                // Scene UBO 已全局化，无需 per-material 注册
    SetLocalToWorld,     // UBO/SSBO LocalToWorld → SetLocalToWorld(stage_bits)
    AddSSBOStruct,       // AddSSBOStruct(stage_bits, *sbs)
    AddSSBOVertex,       // AddSSBOVertex(stage_bits, *sbs)（顶点数据 / MeshDrawParams）
    AddSSBOVertexIndex,  // AddSSBOVertexIndex(stage_bits)
    AddSSBOTextureLayer, // 纹理层行表（binding = 数据槽数）
    AddSSBOMaterialPrivateDataIndex,     // 材质数据行表（固定 binding 常量路径）
};

struct DescriptorRegisterEntry
{
    DescriptorSemantic semantic;
    DescriptorKind kind;
    RegisterOp op;
    const ShaderBufferSource *sbs;   // AddSSBOStruct / AddSSBOVertex 使用
    const char *label;               // 诊断名
};

// 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）——
// PerObject 集固定 binding（kPerObjectBindingVertex*），走固定名路径。
// Scene UBO（ViewportInfo/CameraInfo/SkyInfo/ColorPalette）已全局化（P1/P1-2a），
// 不再进入 per-material 分配器（desc_manager/finalize/绑定均跳过 Scene）。
static const DescriptorRegisterEntry kDescriptorRegisterTable[] = {
    // ── UBO ──
    { DescriptorSemantic::ViewportInfo,           DescriptorKind::UBO,  RegisterOp::None,              nullptr,                "Scene UBO" },
    { DescriptorSemantic::CameraInfo,             DescriptorKind::UBO,  RegisterOp::None,              nullptr,                "Scene UBO" },
    { DescriptorSemantic::SkyInfo,                DescriptorKind::UBO,  RegisterOp::None,              nullptr,                "Scene UBO" },
    { DescriptorSemantic::MaterialColorPalette,   DescriptorKind::UBO,  RegisterOp::None,              nullptr,                "Scene UBO" },
    { DescriptorSemantic::LocalToWorld,           DescriptorKind::UBO,  RegisterOp::SetLocalToWorld,   nullptr,                "LocalToWorld" },
    // ── SSBO ──
    { DescriptorSemantic::LocalToWorld,             DescriptorKind::SSBO, RegisterOp::SetLocalToWorld,   nullptr,                    "LocalToWorld" },
    { DescriptorSemantic::LocalToWorldIndex,   DescriptorKind::SSBO, RegisterOp::AddSSBOStruct,     &SBS_LocalToWorldIndexRows,  "LocalToWorldIndexRows" },
    { DescriptorSemantic::MaterialTextureLayerTable,DescriptorKind::SSBO, RegisterOp::AddSSBOTextureLayer,nullptr,                    "MaterialTextureLayerRows" },
    { DescriptorSemantic::MaterialPrivateDataIndex,   DescriptorKind::SSBO, RegisterOp::AddSSBOMaterialPrivateDataIndex,   nullptr,                    "MaterialPrivateDataIndex" },
    { DescriptorSemantic::VertexPosition,           DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexPosition,        "VertexPosition" },
    { DescriptorSemantic::VertexUV,                 DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexUV,              "VertexUV" },
    { DescriptorSemantic::VertexNTB,                DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexNTB,             "VertexNTB" },
    { DescriptorSemantic::VertexColor,              DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexColor,           "VertexColor" },
    { DescriptorSemantic::VertexLuminance,          DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexLuminance,       "VertexLuminance" },
    { DescriptorSemantic::VertexTransformID,        DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexTransformID,     "VertexTransformID" },
    { DescriptorSemantic::VertexSize,               DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_VertexSize,            "VertexSize" },
    { DescriptorSemantic::MeshDrawParams,           DescriptorKind::SSBO, RegisterOp::AddSSBOVertex,     &SBS_MeshDrawParams,        "MeshDrawParams" },
    { DescriptorSemantic::VertexIndex,              DescriptorKind::SSBO, RegisterOp::AddSSBOVertexIndex,nullptr,                    "VertexIndex" },
};

static const DescriptorRegisterEntry *FindDescriptorRegisterEntry(
    const DescriptorKind kind,
    const DescriptorSemantic semantic)
{
    for (const DescriptorRegisterEntry &reg : kDescriptorRegisterTable)
    {
        if (reg.kind == kind && reg.semantic == semantic)
            return &reg;
    }
    return nullptr;
}

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

        // MaterialPrivateData：MaterialSSBOBuilder 依据 material_private_data_slot_decls 注册，
        // 此处只累加其 stage bits（UBO/SSBO 两分支同语义，先于 kind 判断统一处理）。
        if ((entry.kind == DescriptorKind::UBO || entry.kind == DescriptorKind::SSBO)
         && entry.semantic == DescriptorSemantic::MaterialPrivateData)
        {
            io_material_ssbo_stage_bits = stage_bits;
            continue;
        }

        const DescriptorRegisterEntry *reg =
            FindDescriptorRegisterEntry(entry.kind, entry.semantic);
        if (!reg)
            continue;

        switch (reg->op)
        {
        case RegisterOp::None:
            break;

        case RegisterOp::SetLocalToWorld:
            ctx->SetLocalToWorld(stage_bits);
            break;

        case RegisterOp::AddSSBOStruct:
            if (!ctx->AddSSBOStruct(stage_bits, *reg->sbs))
                return c.Fail(std::string("failed to add ") + reg->label + " SSBO");
            break;

        case RegisterOp::AddSSBOVertex:
            if (!ctx->AddSSBOVertex(stage_bits, *reg->sbs))
                return c.Fail(std::string("failed to add ") + reg->label + " SSBO");
            break;

        case RegisterOp::AddSSBOVertexIndex:
            if (!ctx->AddSSBOVertexIndex(stage_bits))
                return c.Fail(std::string("failed to add ") + reg->label + " SSBO");
            break;

        case RegisterOp::AddSSBOTextureLayer:
            if (!ctx->AddStruct(SBS_MaterialTextureLayerRows.struct_name, ""))
                return c.Fail(std::string("failed to add ") + reg->label + " struct");
            // 单槽化：材质至多一个 MaterialPrivateData 槽，declared 计数为 0 或 1。
            // 纹理层行表紧随数据槽之后（binding = N），无数据槽时 N = 0。
            if (!ctx->AddSSBOTextureLayer(stage_bits, int(declared_material_private_data_slot_count)))
                return c.Fail(std::string("failed to add ") + reg->label + " SSBO");
            break;

        case RegisterOp::AddSSBOMaterialPrivateDataIndex:
            if (!ctx->AddStruct(SBS_MaterialPrivateDataIndexRows.struct_name, ""))
                return c.Fail(std::string("failed to add ") + reg->label + " struct");
            // P1-2c：MaterialPrivateDataIndexRows 迁至 PerObject 集，binding 由固定常量表
            // kPerObjectBinding* 确定（走固定名路径，与 l2w_index_rows 同构）。
            if (!ctx->AddSSBOMaterialPrivateDataIndex(stage_bits))
                return c.Fail(std::string("failed to add ") + reg->label + " SSBO");
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
};

static const CharQuadSSBOReg kCharQuadSSBOTable[] = {
    { "TextCharInfo",     "sbo_char_info" },
    { "CharStyleData",    "sbo_char_style" },
    { "CharInstanceData", "sbo_char_instance" },
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
                          reg.struct_name, reg.sbo_name))
            return c.Fail(std::string("failed to add ") + reg.sbo_name + " SSBO");
    }

    return true;
}

// ── Step 3d: 材质数据槽描述符（material_private_data_slot_decls 驱动）─────────────────────────
// 单槽化：固定 slot 0（DefaultMaterialPrivateDataSlot），至多一个声明。
static bool RegisterMaterialPrivateDataSlotDescriptors(
    ShaderBuildContext *ctx,
    const std::vector<MaterialPrivateDataSlotDeclaration> &material_private_data_slot_decls,
    const uint32_t material_ssbo_stage_bits,
    CompileContext &c)
{
    if (material_private_data_slot_decls.size() > MaxMaterialPrivateDataSlotsPerMaterial)
        return c.Fail("a material may declare at most one MaterialPrivateData slot");

    for (uint32_t i = 0; i < static_cast<uint32_t>(material_private_data_slot_decls.size()); ++i)
    {
        if (!AddMaterialPrivateDataSlotDescriptor(*ctx, material_private_data_slot_decls[i], DefaultMaterialPrivateDataSlot, material_ssbo_stage_bits))
            return c.Fail("failed to add declared material ssbo slot descriptor");
    }

    return true;
}

// ── Step 4: Ensure index-table SSBOs for vertex-varying emissions ────────────
//
//     The descriptor builder calls EnsureMaterialPrivateDataIndexTable only when a
// MaterialPrivateData entry exists.  Compositor materials without data
// slots may still emit fragDataIndexID (declared as a varying in
// .material.toml).  Without the corresponding SSBO the VS GLSL injection
// skips ResolveMaterialPrivateDataIndex → compile error.
static void EnsureIndexTableSSBOs(
    ShaderBuildContext *ctx,
    const CompositorMaterialBuildConfig &config,
    const std::vector<SerializedDescriptorEntry> &descriptor_entries)
{
    if (!config.material_definition)
        return;

    const auto &vv = config.material_definition->vertex_varying;

    bool has_index_table = false;
    for (const SerializedDescriptorEntry &entry : descriptor_entries)
    {
        if (entry.semantic == DescriptorSemantic::MaterialPrivateDataIndex)
        {
            has_index_table = true;
            break;
        }
    }

    // P1-2c：MaterialPrivateDataIndexRows 迁至 Transform 集，binding 由固定常量表确定，
    // 与 Step 3 的 MaterialPrivateDataIndex 分支同构。
    if (vv.emit_data_index_id && !has_index_table)
    {
        ctx->AddStruct(SBS_MaterialPrivateDataIndexRows.struct_name, "");
        ctx->AddSSBO(hgl::graph::kMeshFragment,
                     SBS_MaterialPrivateDataIndexRows.set_type,
                     SBS_MaterialPrivateDataIndexRows.struct_name,
                     SBS_MaterialPrivateDataIndexRows.name);
    }
}

// ── Step 5a: set/binding 宏注入（配表）───────────────────────────────────────
// 显式双宏名（_SET/_BINDING 成对传入）——不做字符串推导：
// 推导依赖 "_SET" 子串存在，缺了即 std::out_of_range。
//
// #ifndef 保护：模块 include 链可能提前引入 descriptor_macros.glsl
// （其 L2W_SET=PER_OBJECT_SET 等宏体文本与这里不同——GLSL 重定义检查
// 比较宏体文本，同值不同体也报错）。先定义者胜——当前布局下两者展开
// 值一致（PER_OBJECT_SET=1=SBS set）；改 SBS 布局时需同步
// descriptor_macros.glsl 的默认值。
//
// 行表绑定（material_private_data_index_rows / mtl_texture_layer_rows / l2w_index_rows）不再
// 注入 set/binding 宏：声明由 index table 生成逻辑依据 descriptor_info 直接以
// layout(set=.., binding=..) 写出（统一声明生成，不再写死在 .glsl）。
struct BindingDefineSpec
{
    const char *set_macro;
    const char *binding_macro;
    DescriptorSetType set_type;      // Scene 固定注入路径使用
    int binding;                     // < 0 表示运行期从 descriptor_info 解析
    const ShaderBufferSource *sbs;   // binding < 0 时用于 GetSSBO(name)
};

static const BindingDefineSpec kBindingDefineTable[] = {
    // 运行期从 per-material 分配器解析 set/binding（固定名路径）
    { "L2W_SET", "L2W_BINDING", DescriptorSetType::PerObject, -1, &SBS_LocalToWorld },
    // mesh per-draw 参数表（IndirectMeshDraw）：GLSL 声明在 MeshShaderAssembler
    // 生成体内，依赖此宏对取实际 set/binding（固定名路径 → PerObject/13）
    { "MESH_DRAW_PARAMS_SET", "MESH_DRAW_PARAMS_BINDING", DescriptorSetType::PerObject, -1, &SBS_MeshDrawParams },
    // ── Scene UBO（camera/sky/viewport/color_palette）已全局化（P1/P1-2a）：
    //    binding 为硬编码常量，不再从 per-material 分配器查询
    //    （Scene 不再进入 per-material 描述符集）。显式注入 _SET/_BINDING 宏，
    //    保证 shader ABI（descriptor_macros.glsl 默认值与此一致）。
    { "VIEWPORT_SET", "VIEWPORT_BINDING", DescriptorSetType::Scene, kSceneBindingViewport, nullptr },
    { "CAMERA_SET", "CAMERA_BINDING", DescriptorSetType::Scene, kSceneBindingCamera, nullptr },
    { "SKY_SET", "SKY_BINDING", DescriptorSetType::Scene, kSceneBindingSky, nullptr },
    { "COLOR_PALETTE_SET", "COLOR_PALETTE_BINDING", DescriptorSetType::Scene, kSceneBindingColorPalette, nullptr },
};

static void AppendBindingDefine(
    std::string &out,
    const char *set_macro,
    const char *binding_macro,
    const int set,
    const int binding)
{
    if (!set_macro || !binding_macro || set < 0 || binding < 0)
        return;
    out += "#ifndef ";
    out += set_macro;
    out += "\n#define ";
    out += set_macro;
    out += " ";
    out += std::to_string(set);
    out += "\n#endif\n";
    out += "#ifndef ";
    out += binding_macro;
    out += "\n#define ";
    out += binding_macro;
    out += " ";
    out += std::to_string(binding);
    out += "\n#endif\n";
}

static std::string BuildBindingPreamble(
    const DescriptorSetLayoutAllocator &descriptor_info)
{
    std::string preamble;

    for (const BindingDefineSpec &spec : kBindingDefineTable)
    {
        if (spec.binding >= 0)
        {
            AppendBindingDefine(preamble, spec.set_macro, spec.binding_macro,
                                int(spec.set_type), spec.binding);
            continue;
        }

        const ShaderDescriptor *sd = descriptor_info.GetSSBO(spec.sbs->name);
        if (!sd)
            continue;
        AppendBindingDefine(preamble, spec.set_macro, spec.binding_macro,
                            sd->set, sd->binding);
    }

    return preamble;
}

// ── Step 5b: Material SSBO GLSL 声明 ─────────────────────────────────────────
// 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
// 统一依据 material_private_data_slot_decls 生成并注入 Fragment 阶段。
// 单槽化：一个材质固定生成一个 buffer（MaterialPrivateData，slot 0）。
static bool BuildMaterialSSBODeclarations(
    const DescriptorSetLayoutAllocator &descriptor_info,
    const std::vector<MaterialPrivateDataSlotDeclaration> *material_private_data_slot_decls,
    CompileContext &c,
    std::string &out_decls,
    std::string &out_macros)
{
    if (!material_private_data_slot_decls || material_private_data_slot_decls->empty())
        return true;

    if (material_private_data_slot_decls->size() > MaxMaterialPrivateDataSlotsPerMaterial)
        return c.Fail("a material may declare at most one MaterialPrivateData slot");

    const MaterialPrivateDataSlotDeclaration &decl = (*material_private_data_slot_decls)[0];
    const ShaderDescriptor *sd = descriptor_info.GetSSBO(decl.name.c_str());
    if (!sd || sd->set < 0 || sd->binding < 0)
        return c.Fail("material ssbo descriptor unresolved for GLSL generation");

    const char *struct_name  = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
    const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(decl.ssbo_type);
    if (!struct_name || !struct_codes)
        return c.Fail("unsupported material ssbo type for GLSL generation");

    const char *const buffer_base =
        ssbo::GetMaterialSSBOBufferName(decl.ssbo_type);
    if (!buffer_base)
        return c.Fail("material ssbo buffer name unsupported for GLSL generation");

    out_decls += "struct ";
    out_decls += struct_name;
    out_decls += "\n{\n";

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
            out_decls += "    ";
            out_decls.append(line, start, line.size() - start);
            out_decls += '\n';
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

    out_decls += "};\n";

    out_decls += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
    out_decls += buffer_base;
    out_decls += " {\n    ";
    out_decls += struct_name;
    out_decls += " data[];\n} ";
    out_decls += decl.name;
    out_decls += ";\n";

    out_macros += "#define MTL_DATA_SLOT_COUNT 1u\n";
    out_macros += "#define MTL_DATA ";
    out_macros += decl.name;
    out_macros += "\n";

    return true;
}

// ── Step 5c: 编译期宏（compile_defines）──────────────────────────────────────
// 遍历 MaterialDefinition.compile_defines，为每个名字生成 "#define <name> 1\n"。
// 用于在 GLSL 中通过 #ifdef 切换代码路径（如 TEXT_SDF_ENABLED）。
static std::string BuildCompileDefineMacros(
    const CompositorMaterialBuildConfig &config)
{
    std::string macros;
    if (!config.material_definition)
        return macros;

    for (const auto &name : config.material_definition->compile_defines)
    {
        if (name.empty())
            continue;
        macros += "#define ";
        macros += name;
        macros += " 1\n";
    }
    return macros;
}

// ── Step 5d: Instance index table SSBO GLSL 声明 ─────────────────────────────
// material_private_data_index_rows / mtl_texture_layer_rows / l2w_index_rows 的 buffer
// 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
// descriptor_info 生成注入：VS 阶段提供 l2w_index_rows / material_private_data_index_rows
//（含 ResolveTransformID / ResolveMaterialPrivateDataIndex），FS 阶段提供
// mtl_texture_layer_rows（named-slot TextureLayerRowsData，见下方注入）。
struct IndexTableSpec
{
    const char *sbs_name;        // descriptor_info 查询键（SBS_*.name）
    const char *buffer_name;
    const char *var_name;
    const char *resolve_func;    // 为空则仅生成 buffer 声明
};

static const IndexTableSpec kVSIndexTableSpecs[] = {
    { SBS_LocalToWorldIndexRows.name, "LocalToWorldIndexRows", "l2w_index_rows",     "ResolveTransformID" },
    { SBS_MaterialPrivateDataIndexRows.name, "MaterialPrivateDataIndex", "mtl_private_data_index", "ResolveMaterialPrivateDataIndex" },
};

static void AppendIndexTableDecl(
    std::string &out,
    const ShaderDescriptor *sd,
    const IndexTableSpec &spec)
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
        // 单槽化：行表写单列（values[iid]），不再按 slot 索引。
        out += "uint ";
        out += spec.resolve_func;
        out += "(uint iid) { return ";
        out += spec.var_name;
        out += ".values[iid]; }\n";
    }
}

static std::string BuildVSIndexTableDecls(
    const DescriptorSetLayoutAllocator &descriptor_info)
{
    // 单槽化：MTL_DATA_SLOT_COUNT 恒 1（MaterialPrivateDataIndexRows 索引 0 仍有效）
    std::string out = "#define MTL_DATA_SLOT_COUNT 1u\n";

    for (const IndexTableSpec &spec : kVSIndexTableSpecs)
        AppendIndexTableDecl(out, descriptor_info.GetSSBO(spec.sbs_name), spec);

    return out;
}

static std::string BuildFSIndexTableDecls(
    const DescriptorSetLayoutAllocator &descriptor_info)
{
    std::string out;

    // FS 阶段注入 bindless 纹理行表：TextureLayerRowsData struct + buffer（named slot）。
    // 字段名 = TextureSlot 的 snake_case 名（GetTextureSlotName），顺序与枚举一致；
    // 内存布局与旧扁平 values[RANGE_SIZE] 逐字节相同，故 CPU 上传无需改动。
    // 行索引即 fragDataIndexID（P1-2e：TextureLayerID varying 已删除）。
    // 仅当该材质确实注册了 mtl_texture_layer_rows（存在 MaterialTextureLayerTable
    // 描述符，即声明了纹理槽）时才注入 named-slot struct + buffer；否则跳过
    //（与 AppendIndexTableDecl 的静默跳过行为一致，无纹理槽材质 FS 不引用该 buffer）。
    const ShaderDescriptor *sd =
        descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name);
    if (sd && sd->set >= 0 && sd->binding >= 0)
    {
        out += "struct TextureLayerRowsData\n{\n";
        for (uint32_t i = 0;
             i < static_cast<uint32_t>(TextureSlot::RANGE_SIZE); ++i)
        {
            out += "    uint ";
            out += GetTextureSlotName(static_cast<TextureSlot>(i));
            out += ";\n";
        }
        out += "};\n";
        out += "layout(set=" + std::to_string(sd->set)
              + ", binding=" + std::to_string(sd->binding)
              + ") readonly buffer TextureLayerRowsBuffer\n{\n"
              + "    TextureLayerRowsData data[];\n"
              + "} mtl_texture_layer_rows;\n";
    }

    return out;
}

// ── Step 5e: 最终 GLSL 组装 ──────────────────────────────────────────────────
// GLSL requires #version to be the very first token.
static std::string InsertAfterVersionLine(const std::string &glsl, const std::string &inject)
{
    if (inject.empty())
        return glsl;
    const auto pos = glsl.find('\n');
    if (pos == std::string::npos)
        return glsl + "\n" + inject;
    return glsl.substr(0, pos + 1) + inject + glsl.substr(pos + 1);
}

static std::string InsertBeforeSurfaceFunction(const std::string &glsl, const std::string &inject)
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
}

static void AssembleFinalGLSL(
    ShaderBuildContext *ctx,
    const std::string &ms_glsl,
    const std::string &fs_glsl,
    const ModuleResourceManifest *manifest,
    const std::string &binding_preamble,
    const std::string &vs_index_table_decls,
    const std::string &compile_define_macros,
    const std::string &sampler_macros,
    const std::string &material_ssbo_decls,
    const std::string &material_slot_macros,
    const std::string &fs_index_table_decls)
{
    std::string ms_final = InsertAfterVersionLine(ms_glsl, binding_preamble + vs_index_table_decls);

    // 注入顺序（InsertAfterVersionLine 后注入者位于 version 后更前）：
    // 模块代码可能引用 SSBO 类型、MTL_DATA 宏、索引行表与采样器宏
    // （unlit_source 的 EmissiveSurfaceData / MTL_DATA；bindless_textures 的
    // mtl_texture_layer_rows / TrilinearSampler）——全部注入统一放最后
    // （version 后最前，先于模块代码）。T4.1：一次性组装，顺序在单个串里
    // 显式可见（宏/声明在前、模块代码在后），不再依赖多次调用的反向次序。
    std::string fs_final = fs_glsl;
    const std::string code_module_glsl = BuildCodeModuleGLSL(manifest);

    // FS 统一启用 GL_EXT_mesh_shader：per-primitive 语义（DataIndexID/StyleID）的
    // FS 输入用 perprimitiveEXT in（mesh out perprimitiveEXT → FS in 必须同装饰，
    // 否则 VUID-RuntimeSpirv-OpVariable-08746 接口装饰不匹配）。
    // 扩展声明必须在 #version 之后（InsertAfterVersionLine 保证）。
    std::string fs_injects = "#extension GL_EXT_mesh_shader : require\n"
        + binding_preamble + compile_define_macros + sampler_macros + material_ssbo_decls
        + material_slot_macros + fs_index_table_decls;

    if (!code_module_glsl.empty())
    {
        if (fs_glsl.find("#include SURFACE_FUNCTION_FILE") != std::string::npos)
            // marker 模板：模块代码插到 surface function 前（独立于 version 后注入组）
            fs_final = InsertBeforeSurfaceFunction(fs_final, code_module_glsl);
        else
            fs_injects += code_module_glsl;   // 排在注入组尾部（保持原顺序：宏/声明在前、模块代码在后）
    }

    fs_final = InsertAfterVersionLine(fs_final, fs_injects);

    ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
    ShaderCreateInfo *frag = ctx->GetStageShader(ShaderStage::Fragment);

    // mesh shader 材质：ms_glsl 实为 mesh stage 源码，
    // 设到 mesh ShaderCreateInfo。
    if (mesh)
        mesh->SetFinalGLSL(ms_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);
}

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
        return nullptr;

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
    if (!CreateBuildContext(profile, config, shader_stage_bits, with_local_to_world, c))
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
    // Provider metadata contributes material SSBO slots to the same canonical
    // declaration list as the material definition.
    std::vector<MaterialPrivateDataSlotDeclaration> effective_material_private_data_slot_decls;
    if (!MergeMaterialPrivateDataSlotDeclarations(config, c, effective_material_private_data_slot_decls))
        return FailCompile(c);

    const std::vector<MaterialPrivateDataSlotDeclaration> *material_private_data_slot_decls =
        effective_material_private_data_slot_decls.empty() ? nullptr : &effective_material_private_data_slot_decls;

    DescriptorContract effective_descriptor_contract{};
    std::vector<SerializedDescriptorEntry> descriptor_entries;
    uint32_t declared_material_private_data_slot_count = 0;
    if (!BuildEffectiveDescriptorEntries(
            config, base_descriptor_contract, material_private_data_slot_decls,
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
     && config.material_definition->mesh_shader_mode == MeshShaderMode::CharQuad)
    {
        if (!RegisterCharQuadSSBOs(ctx, shader_stage_bits, c))
            return FailCompile(c);
    }

    if (material_private_data_slot_decls
     && !RegisterMaterialPrivateDataSlotDescriptors(
            ctx, *material_private_data_slot_decls, material_ssbo_stage_bits, c))
        return FailCompile(c);

    // ── Step 4: Ensure index-table SSBOs for vertex-varying emissions ──
    EnsureIndexTableSSBOs(ctx, config, descriptor_entries);

    // ── Step 5: Set complete GLSL (bypass ProcXXX pipeline) ───────
    const DescriptorSetLayoutAllocator &descriptor_info = ctx->GetDescriptorAllocator();

    const std::string binding_preamble = BuildBindingPreamble(descriptor_info);

    std::string material_ssbo_decls;
    std::string material_slot_macros;
    if (!BuildMaterialSSBODeclarations(descriptor_info, material_private_data_slot_decls, c,
                                       material_ssbo_decls, material_slot_macros))
        return FailCompile(c);

    // ── Sampler 预设宏（统一注册机制）──────────────────────────────
    // 遍历 MaterialDefinition.sampler_names，为每个名字生成 "#define <name>Sampler <idx>u"。
    // idx 经 SamplerPresetLibrary 查询（查不到保底 0），与运行时 binding=1 数组下标一致。
    std::string sampler_macros;
    if (config.material_definition)
        sampler_macros = BuildSamplerMacros(config.material_definition->sampler_names);

    const std::string compile_define_macros = BuildCompileDefineMacros(config);

    // 单槽化：MTL_DATA_SLOT_COUNT 恒 1（MaterialPrivateDataIndexRows 索引 0 仍有效）
    const std::string vs_index_table_decls =
        BuildVSIndexTableDecls(descriptor_info);
    const std::string fs_index_table_decls =
        BuildFSIndexTableDecls(descriptor_info);

    AssembleFinalGLSL(ctx, ms_glsl, fs_glsl, config.resource_manifest,
                      binding_preamble, vs_index_table_decls,
                      compile_define_macros, sampler_macros,
                      material_ssbo_decls, material_slot_macros,
                      fs_index_table_decls);

    // ── Step 6: Build ShaderResourceSchema from descriptor entries. ──
    // When material_private_data_slot_decls is provided, material SSBO entries are
    // generated from it and merged with the canonical descriptor entries.
    ShaderResourceSchema shader_resource_schema;
    if (!BuildAndValidateResourceSchema(effective_descriptor_contract, config, c,
                                        shader_resource_schema))
        return FailCompile(c);

    ctx->SetShaderResourceSchema(shader_resource_schema);

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
