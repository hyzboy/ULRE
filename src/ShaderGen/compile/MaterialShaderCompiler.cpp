/// MaterialShaderCompiler.cpp — canonical material input compiler
///
/// 流程（BDA 终态，描述符分配器已退役）：
///   1. 求解层构建契约/schema（数据槽信号经 requires_runtime_data_rows 直判）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/log/Log.h>
#include "compile/MaterialShaderEmitter.h"
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {
namespace
{
/// 命中后校验：落盘的 generated_source_digest 必须与本轮实际生成的
/// GLSL 文本一致。
///
/// 不一致说明「声明键欠近似」——文本变了，但由声明源算出的 6 字段
/// stage key 没变，缓存键因此把两份不同的 GLSL 混成了同一个。
/// 这种情况下缓存的 SPV 不可信，必须当作未命中重编译；同时这条日志
/// 是声明键忠实度的**运行时**证据，比构建期的对齐诊断更接近真实输入。
bool VerifyGeneratedSourceDigest(
    const ShaderCreateInfo &mesh,
    const ShaderCreateInfo &fragment,
    const uint64 stored_source_digest,
    const ShaderLinkSpec &link)
{
    const std::string &mesh_glsl = mesh.GetFinalGLSL();
    const std::string &fragment_glsl = fragment.GetFinalGLSL();

    if (MatchesGeneratedSourceDigest(
            mesh_glsl.data(), mesh_glsl.size(),
            fragment_glsl.data(), fragment_glsl.size(),
            stored_source_digest))
        return true;

    GLogWarning(u8"[ShaderArtifactStore] program %s 命中后校验失败："
                u8"落盘文本摘要=%llu —— 声明键欠近似，"
                u8"丢弃缓存重编译",
                ToU8String(link.BuildKey().ToString()).c_str(),
                static_cast<unsigned long long>(stored_source_digest));
    return false;
}
} // namespace

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
        uint64 stored_source_digest = 0;
        cache_hit = artifact_store->LoadProgramArtifacts(
            link,
            build_spec->GetProgramArtifactMetadata(),
            cached_vertex,
            cached_fragment,
            &stored_source_digest);
        if (cache_hit)
        {
            // mesh shader 材质：顶点处理 stage 是 Mesh
            ShaderCreateInfo *vertex =
                build_spec->GetStageShader(ShaderStage::Mesh);
            ShaderCreateInfo *fragment =
                build_spec->GetStageShader(ShaderStage::Fragment);
            // 命中后校验必须在 SetCachedSPVData 之前：文本摘要来自本轮
            // 生成的 GLSL，校验不过就不能让缓存 SPV 顶替掉它。
            cache_hit = vertex
                    && fragment
                    && VerifyGeneratedSourceDigest(
                           *vertex, *fragment, stored_source_digest, link)
                    && vertex->SetCachedSPVData(
                           cached_vertex.GetData(),
                           cached_vertex.GetCount())
                    && fragment->SetCachedSPVData(
                           cached_fragment.GetData(),
                           cached_fragment.GetCount());
        }
        build_spec->SetUsedCachedProgram(cache_hit);
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



// ═══════════════════════════════════════════════════════════════════════════
// BuildMaterialStageDocuments — Compositor 模板完整 GLSL → ShaderBuildContext
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════════
// BuildMaterialStageDocuments 内部辅助实现
//
// BuildMaterialStageDocuments 的 7 步流水线被拆分为多个静态辅助函数，
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
            "[BuildMaterialStageDocuments] material=%s failed: %s\n",
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



// ── Step 5a: set/binding 宏 ──────────────────────────────────────────────────
// 固定 ABI 的 set/binding 宏（L2W/MESH_DRAW_PARAMS/VIEWPORT/CAMERA/SKY/COLOR_PALETTE
// 及顶点系列）不再由编译器注入：descriptor_macros.glsl 为生成物
//（DescriptorMacroGen，数值真源 DescriptorSetTypeDef.h 的绑定枚举），模板与模块
// #include 后直接使用默认值，单一真源。
//
// 行表（material_data_addresses / l2w_index）无 set/binding 概念：
// BDA 化后行表经 pc_root + buffer_reference 寻址，声明由 index table 生成器恒发射。


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

ShaderBuildContext *BuildMaterialStageDocuments(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const MaterialCompileConfig &config)
{
    return BuildMaterialStageDocuments(
        profile, input, mesh_source_document, fragment_source_document, config, nullptr);
}

ShaderBuildContext *BuildMaterialStageDocuments(
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

        const char *missing_sampler = nullptr;
        if (!ValidateShaderCodeResourceManifestSamplerReferences(
                *config.resource_manifest,
                *config.material_definition,
                &missing_sampler))
        {
            c.Fail(std::string("Resource manifest requires sampler preset '")
                   + (missing_sampler ? missing_sampler : "<null>")
                   + "' that the material does not declare in "
                     "[resources].samplers");
            return FailCompile(c);
        }
    }

    const uint32_t shader_stage_bits = config.shader_stage_flag_bits != 0
        ? config.shader_stage_flag_bits
        : uint32_t(ShaderStage::MeshFragment);

    // ── Step 2: Create ShaderBuildContext ─────────────────────────
    if (!CreateBuildContext(config, shader_stage_bits, c))
        return FailCompile(c);

    ShaderBuildContext *ctx = c.ctx;

    // ── Step 3: 材质私有数据 SSBO 类型（编译配置单一声明）──
    GlobalSSBOType effective_material_private_data = GlobalSSBOType::PBRSurface;
    if (!ResolveEffectiveMaterialPrivateData(config, c, effective_material_private_data))
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


    // ── Step 6: 运行时数据行需求（编译期直判，唯一活信号）──
    // 2026-10-11（S1）：原资源条目载体与其校验已整体删除；这里只保留
    // 「本材质是否需要 per-instance 运行时数据行」这一条编译期直判信号。
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
    const bool requires_recipe_runtime_rows =
        has_material_ssbo_payload
        || (config.material_definition
         && (config.material_definition->vertex_varying.emit_data_index_id
          || has_active_texture_reference_consumer));

    ctx->SetRequiresRecipeRuntimeRows(requires_recipe_runtime_rows);

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
