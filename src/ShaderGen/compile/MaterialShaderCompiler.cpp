/// MaterialShaderCompiler.cpp — canonical material input compiler
///
/// 流程（BDA 终态，描述符分配器已退役）：
///   1. 求解层构建契约/schema（数据槽信号经 requires_runtime_data_rows 直判）
///   2. 使用 SetFinalGLSL；Finalize 时独立读取 stage 缓存并只编译 miss

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
#include <hgl/mtl/ShaderCacheRoot.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/type/StdString.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/common/DebugChannel.h>
#include <algorithm>
#include <atomic>
#include <cstdlib>

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {
// 发射层（文件末尾）里的判定函数，供本文件较早的调用点前向可见。
bool MaterialDefinitionRequiresPayloadRow(
    GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    bool &out_required) noexcept;

namespace
{
bool LoadCachedStage(
    ShaderArtifactStore *store,
    const ShaderStageKey &key,
    const ShaderCreateInfo *shader,
    ValueArray<hgl::uint8> &cached_spv)
{
    if (!store || !shader)
        return false;

    const std::string &glsl = shader->GetFinalGLSL();
    const uint64 expected_digest =
        ComputeGeneratedStageSourceDigest(glsl.data(), glsl.size());
    uint64 stored_digest = 0;
    if (expected_digest != 0
     && store->LoadStageSPV(key, cached_spv, &stored_digest)
     && stored_digest == expected_digest)
        return true;

    if (stored_digest != 0 && stored_digest != expected_digest)
        GLogWarning(
            u8"[ShaderArtifactStore] stage %s 命中后校验失败："
            u8"source digest 不匹配，按 miss 重新编译",
            ToU8String(key.ToString()).c_str());
    cached_spv.Clear();
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

    bool mesh_cache_hit = false;
    bool fragment_cache_hit = false;
    ShaderCreateInfo *mesh =
        build_spec->GetStageShader(ShaderStage::Mesh);
    ShaderCreateInfo *fragment =
        build_spec->GetStageShader(ShaderStage::Fragment);
    if (!mesh || !fragment)
        return false;

    if (artifact_store)
    {
        const auto &link = build_spec->GetProgramLink();
        ValueArray<hgl::uint8> cached_vertex;
        ValueArray<hgl::uint8> cached_fragment;
        mesh_cache_hit = LoadCachedStage(
            artifact_store, link.mesh_stage, mesh, cached_vertex)
            && mesh->SetCachedSPVData(
                cached_vertex.GetData(), cached_vertex.GetCount());
        fragment_cache_hit = LoadCachedStage(
            artifact_store, link.fragment_stage, fragment, cached_fragment)
            && fragment->SetCachedSPVData(
                cached_fragment.GetData(), cached_fragment.GetCount());
    }
    build_spec->SetUsedCachedStages(mesh_cache_hit, fragment_cache_hit);

    // ULRE_DEBUG.toml 的 channels.shadergen = true：落盘每次材质构建的最终 GLSL（mesh+fs）。
    // 输出目录由 DumpShaderGenGLSL 解析（<ShaderCacheRoot>/glsldump，不绑定机器路径）。
    // 运行时可开关，Release 同样有效 —— 这里的调用本身不判断，判断在 DumpShaderGenGLSL 内部。
    if (const ShaderCreateInfo *mesh_si =
            build_spec->GetStageShader(ShaderStage::Mesh))
        DumpShaderGenGLSL("mesh", mesh_si->GetFinalGLSL());

    if (const ShaderCreateInfo *frag_si =
            build_spec->GetStageShader(ShaderStage::Fragment))
        DumpShaderGenGLSL("fs", frag_si->GetFinalGLSL());
    if ((!mesh_cache_hit || !fragment_cache_hit)
     && artifact_store
     && artifact_store->GetCacheMode() == ShaderCacheMode::ReadOnly)
        return false;

    if (!build_spec->CreateShaderDirect())
        return false;

    if (artifact_store)
    {
        const auto &link = build_spec->GetProgramLink();
        const uint64 mesh_source_digest =
            ComputeGeneratedStageSourceDigest(
                mesh->GetFinalGLSL().data(), mesh->GetFinalGLSL().size());
        const uint64 fragment_source_digest =
            ComputeGeneratedStageSourceDigest(
                fragment->GetFinalGLSL().data(),
                fragment->GetFinalGLSL().size());
        if ((!mesh_cache_hit && !artifact_store->SaveStageSPV(
                link.mesh_stage,
                mesh->GetSPVData(),
                mesh->GetSPVSize(),
                mesh_source_digest))
         || (!fragment_cache_hit && !artifact_store->SaveStageSPV(
                link.fragment_stage,
                fragment->GetSPVData(),
                fragment->GetSPVSize(),
                fragment_source_digest)))
            return false;

        if (!artifact_store->SaveProgramMetadata(
                link,
                build_spec->GetProgramArtifactMetadata()))
            GLogWarning(
                u8"[ShaderArtifactStore] failed to save program link metadata: %s",
                ToU8String(link.BuildKey().ToString()).c_str());

        // 生成的最终 GLSL 源随 SPV 一起落盘：与 stage SPV 缓存文件
        // 同样的主文件名（同目录，扩展名按 stage 定——.mesh/.frag），
        // 便于日后对照分析。Best-effort：失败只记日志，不阻断 shader 缓存。
        const std::string &mesh_glsl = mesh->GetFinalGLSL();
        if (!mesh_glsl.empty())
            artifact_store->SaveStageGLSL(link.mesh_stage,
                                          mesh_glsl.data(),
                                          mesh_glsl.size());
        const std::string &fragment_glsl = fragment->GetFinalGLSL();
        if (!fragment_glsl.empty())
            artifact_store->SaveStageGLSL(
                link.fragment_stage,
                fragment_glsl.data(),
                fragment_glsl.size());
    }

    return true;
}



// ═══════════════════════════════════════════════════════════════════════════
// BuildMaterialStageDocuments — 阶段源 → ShaderBuildContext
//
// 片段阶段：`config.fixed_fragment_source` 非空时**原样直通**（领域主着色器就是
// 最终文本，没有占位符、没有代码模块拼装），此处只负责编译成 SPV + 反射。
// 网格阶段：仍接收 `BuildMaterialStageDocument` 拼出的文档（阶段六的对象）。
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

    const bool has_fixed_mesh_source =
        config.fixed_mesh_source && config.fixed_mesh_source[0];
    if ((mesh_source_document.GetBlockCount() == 0) != has_fixed_mesh_source
     || (fragment_source_document.GetBlockCount() == 0
      && !config.fixed_fragment_source))
    {
        c.Fail("mesh stage must have exactly one source (document or fixed GLSL)");
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
    if (!has_fixed_mesh_source
     && !BuildMaterialStageDocument(
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
    // 片段源已物理固定（领域主着色器）时不做文档合并：那份文本本身就是完整
    // 编译单元，再套一层 version/define 前缀会写出重复指令。
    if (!config.fixed_fragment_source
     && !BuildMaterialStageDocument(
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
    if (has_fixed_mesh_source)
        mesh_final_glsl = config.fixed_mesh_source;
    else if (!mesh_final_document->Serialize(mesh_final_glsl, document_diagnostics))
    {
        c.Fail("Material stage document serialization failed");
        return FailCompile(c);
    }
    if (config.fixed_fragment_source)
        fragment_final_glsl = config.fixed_fragment_source;
    else if (!fragment_final_document->Serialize(fragment_final_glsl, document_diagnostics))
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

    // ── payload 行需求（合并信号里的一类，单独留痕给运行期）──
    // `requires_recipe_runtime_rows` 为真不代表需要 payload 行：纯纹理材质只需要
    // `MaterialInstanceAddresses` 行表 + 纹理引用行，payload_index 从不被读取。
    // 运行期 `material_ssbo_binding` 校验若读合并信号，会把这类材质误判为缺绑定。
    // 判据与发射层同源（模块元数据 `require Resource MaterialData`），在此折成
    // 程序级信号，运行期不再按定义反推。
    bool requires_recipe_payload_row = false;
    if (!MaterialDefinitionRequiresPayloadRow(
            effective_material_private_data,
            config.material_definition,
            requires_recipe_payload_row))
    {
        c.Fail("Material payload row requirement query failed");
        return FailCompile(c);
    }

    ctx->SetRequiresRecipePayloadRow(requires_recipe_payload_row);

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


// ═══════════════════════════════════════════════════════════════════════════
// 发射层（原 MaterialShaderEmitter.cpp，2026 阶段5.2-g 并入本文件）
//
// 纯函数、零决策：只把求解层已解出的状态转成 ShaderDocument / GLSL 文本。
// 无生成器终态下，仍有消费者的只剩：DumpShaderGenGLSL / BuildSamplerMacros /
// MaterialDefinitionRequiresPayloadRow / BuildMaterialStageDocument（网格阶段）。
// ═══════════════════════════════════════════════════════════════════════════

static std::string BuildMeshIndexTableDecls();
static std::string BuildFSIndexTableDecls(bool fs_has_runtime_rows);
// ── 诊断用 GLSL 落盘（声明见 MaterialShaderCompiler.h）────────────────────────
// 开关：ULRE_DEBUG.toml 的 channels.shadergen = true。运行时可开关，Release 同样有效
// ——排障往往就发生在 Release 跑批上，编译期裁掉等于没有。
void DumpShaderGenGLSL(const char *stage, const std::string &text)
{
    if (!stage || text.empty())
        return;

    if (!ULRE_DEBUG_ON(ShaderGen))
        return;

    const OSString root = GetShaderCacheRootPath();
    if (root.IsEmpty())
        return;

    filesystem::Path directory(root);
    directory /= ToOSString("glsldump");
    const OSString directory_path = directory.ToOSString();

    if (!filesystem::IsDirectory(directory_path)
     && !filesystem::MakePath(directory_path))
        return;

    static std::atomic<uint32_t> dump_seq{0};
    const uint32_t seq = dump_seq.fetch_add(1);

    std::string file_name = "ulre_dump_";
    file_name += std::to_string(seq);
    file_name += "_";
    file_name += stage;
    file_name += ".glsl";

    filesystem::Path file_path(directory_path);
    file_path /= ToOSString(file_name);

    filesystem::SaveMemoryToFile(
        file_path.ToOSString(),
        text.data(),
        static_cast<int64>(text.size()));
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

// 材质源是否读取 payload 行（MTL_ROW）——由模块元数据判定，而不是靠模块名
// 里是否含 "texture_source" 反推（旧实现的 std::strstr 会误命中任何名字里
// 含该子串的模块，且无法表达新模块的诉求）。
//
// 判定依据：模块注解声明 `require Resource MaterialData`，即它会读材质数据行。
//   有（如 pbr_surface_source，用 MTL_ROW）→ true
//   无（如 texture_source，只用 MTL_TEX）  → false
static bool MaterialSourceRequiresMaterialDataRow(
    const char *module_name, bool &out_required) noexcept
{
    const ShaderCodeModuleDefinition *module =
        GetShaderCodeModuleRegistry().FindByName(module_name);
    if (!module)
    {
        GLogError(u8"[MaterialShaderCompiler] Material source module not registered: %s",
                  module_name);
        return false;
    }

    out_required = false;
    for (uint32 i = 0; i < module->semantic_requirement_count; ++i)
    {
        const ShaderCodeModuleSemanticRequirement &requirement =
            module->semantic_requirements[i];

        if (requirement.source == ShaderCodeModuleCapabilitySource::Resource
         && requirement.semantic == ShaderCodeModuleSemantic::MaterialData)
        {
            out_required = true;
            break;
        }
    }

    return true;
}

// 单个 provider 槽位的 payload 行需求。
// 模块名缺省（nullptr/空串）= 该槽位没有模块，自然不读 payload 行；
// 只有「模块名存在但注册表查不到」才算查询失败。
static bool MaterialProviderRequiresPayloadRow(
    const char *module_name, bool &out_required) noexcept
{
    out_required = false;
    if (!module_name || !module_name[0])
        return true;

    return MaterialSourceRequiresMaterialDataRow(module_name, out_required);
}

bool MaterialDefinitionRequiresPayloadRow(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    bool &out_required) noexcept
{
    out_required = false;
    if (!IsGlobalSSBOType(material_private_data))
        return true;

    if (!material_definition)
    {
        // 连定义都没有 ⇒ 无从判定，保守按「需要 payload 行」处理。
        out_required = true;
        return true;
    }

    // 判据只有一条：材质提供的 GLSL 模块里是否声明了
    // `@ulre require Resource MaterialData`（= 真的会读 payload 行/MTL_ROW）。
    // 与「有没有纹理声明」无关 —— 纹理引用行（MTL_TEX）与 payload 行（MTL_ROW）
    // 是两类互不相交的需求，无纹理材质（VertexColor / Checkerboard / 纯 Surface
    // 天空）同样不读 payload 行，因而也不该向 recipe 索要 material_ssbo_binding。
    // （旧实现在无纹理声明时直接返回 true，把「无纹理」当成「一定读 payload」，
    //  误杀了 VertexColor / VertexPaletteColor / Checkerboard2D/3D 一类材质。）
    bool source_requires = false;
    if (!MaterialProviderRequiresPayloadRow(
            material_definition->material_source_module_name,
            source_requires))
        return false;

    bool surface_requires = false;
    if (!MaterialProviderRequiresPayloadRow(
            material_definition->surface_module_name,
            surface_requires))
        return false;

    out_required = source_requires || surface_requires;
    return true;
}

// ── Step 5b: Material SSBO GLSL 声明 ─────────────────────────────────────────
// 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
// 统一依据单一 material_private_data 声明生成并注入 Fragment 阶段。
// 变量名固定为 DefaultMaterialPrivateDataName。
bool BuildMaterialSSBODeclarations(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    const MaterialTextureReferenceLayout *texture_layout,
    std::string &out_decls,
    std::string &out_macros,
    std::string &out_error)
{
    bool has_payload = false;
    if (!MaterialDefinitionRequiresPayloadRow(
            material_private_data, material_definition, has_payload))
    {
        out_error = "material source module is not registered";
        return false;
    }
    const bool has_texture_references =
        texture_layout && texture_layout->HasReferences();
    if (!has_payload && !has_texture_references)
        return true;

    // payload 行和纹理引用行都通过 BDA 地址解引用。
    out_decls += "#extension GL_EXT_buffer_reference : require\n";
    out_decls += "#extension GL_ARB_gpu_shader_int64 : require\n";

    if (has_payload)
    {
        const char *struct_name  = ssbo::GetGlobalSSBOStructName(material_private_data);
        const char *row_struct = ssbo::GetGlobalSSBORowName(material_private_data);
        const char *struct_codes = ssbo::GetGlobalSSBOStructGLSL(material_private_data);
        if (!row_struct || !struct_codes || !struct_name)
        {
            out_error = "unsupported material row type for GLSL generation";
            return false;
        }

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

        out_decls += "struct ";
        out_decls += struct_name;
        out_decls += "\n{\n";

        for (; *p; ++p)
        {
            if (*p == '\n')
                FlushFieldLine();
            else
                line += *p;
        }
        FlushFieldLine();

        out_decls += "};\n";

        out_decls += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer ";
        out_decls += row_struct;
        out_decls += "\n{\n";

        p = struct_codes;
        for (; *p; ++p)
        {
            if (*p == '\n')
                FlushFieldLine();
            else
                line += *p;
        }
        FlushFieldLine();

        out_decls += "};\n";

        const char *ubo_field = nullptr;
        switch (material_private_data)
        {
        case GlobalSSBOType::PBRSurface:          ubo_field = "addr_pbr_surface"; break;
        case GlobalSSBOType::EmissiveSurface:     ubo_field = "addr_emissive_surface"; break;
        case GlobalSSBOType::TransmissionSurface: ubo_field = "addr_transmission_surface"; break;
        default:                                    ubo_field = nullptr; break;
        }
        if (!ubo_field)
        {
            out_error = "unsupported material private data for UBO mapping";
            return false;
        }

        const uint32_t stride = GetGlobalSSBOTypeStructStride(material_private_data);

        out_macros += "#define MTL_ROW(i) ";
        out_macros += row_struct;
        out_macros += "(global_addresses.";
        out_macros += ubo_field;
        out_macros += " + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(";
        out_macros += std::to_string(stride);
        out_macros += "))\n";
    }

    if (has_texture_references)
    {
        if (!material_definition
         || material_definition->texture_declarations.size()
               != texture_layout->reference_count)
        {
            out_error = "texture reference layout declarations are incomplete";
            return false;
        }

        out_decls +=
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef\n";
        out_decls += "{\n";
        for (const auto &declaration :
            material_definition->texture_declarations)
        {
            out_decls += "    uvec2 tex_";
            out_decls += declaration.name;
            out_decls += ";\n";
        }
        out_decls += "};\n";

        out_macros += "#define MTL_TEX(i) ";
        out_macros += "MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(";
        out_macros += std::to_string(texture_layout->row_stride);
        out_macros += "))\n";
    }

    return true;
}

bool BuildMaterialResourceDocument(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    ShaderDocument &out_document,
    std::string &out_error)
{
    out_document.Clear();
    out_error.clear();

    MaterialTextureReferenceLayout texture_layout{};
    const MaterialTextureReferenceLayout *texture_layout_ptr = nullptr;
    if (material_definition)
    {
        if (!BuildMaterialTextureReferenceLayout(
               *material_definition,
               texture_layout))
        {
            out_error = "invalid material texture reference layout";
            return false;
        }
        texture_layout_ptr = &texture_layout;
    }

    std::string declarations;
    std::string macros;
    if (!BuildMaterialSSBODeclarations(
            material_private_data,
            material_definition,
            texture_layout_ptr,
            declarations,
            macros,
            out_error))
        return false;

    ShaderDocumentSource source;
    source.stage = "fragment";
    if (!declarations.empty())
    {
        source.logical_name = "MaterialSSBO";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(declarations.c_str()),
            source);
    }
    if (!macros.empty())
    {
        source.logical_name = "MaterialSSBOAlias";
        out_document.Add(
            ShaderDocumentBlockKind::Define,
            AnsiString(macros.c_str()),
            source);
    }

    const std::string mesh_index_tables =
        BuildMeshIndexTableDecls();
    if (!mesh_index_tables.empty())
    {
        source.logical_name = "MaterialMeshIndexTables";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(mesh_index_tables.c_str()),
            source);
    }

    const std::string fragment_index_tables =
        BuildFSIndexTableDecls(
            IsGlobalSSBOType(material_private_data)
         || (texture_layout_ptr && texture_layout_ptr->HasReferences()));
    if (!fragment_index_tables.empty())
    {
        source.logical_name = "MaterialFragmentIndexTables";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(fragment_index_tables.c_str()),
            source);
    }
    return true;
}

bool BuildCompileDefineDocument(
    const MaterialCompileConfig &config,
    ShaderDocument &out_document)
{
    out_document.Clear();
    std::string macros;
    if (!config.material_definition)
        return true;

    for (const auto &name : config.material_definition->compile_defines)
    {
        if (name.empty())
            continue;
        macros += "#define ";
        macros += name;
        macros += " 1\n";
    }

    if (macros.empty())
        return true;

    ShaderDocumentSource source;
    source.logical_name = "MaterialCompileDefines";
    out_document.Add(
        ShaderDocumentBlockKind::Define,
        AnsiString(macros.c_str()),
        source);
    return true;
}

// ── Step 5d: Instance index table 行表 GLSL 声明 ─────────────────────────────
// l2w_index 行表声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，
// 统一由本生成器注入（A6-2b 去契约门后恒发射）：mesh 阶段提供 l2w_index
//（ResolveTransformID）；FS 行表描述符已随 BDA 化退场（纹理句柄随数据槽行尾下发）。
namespace
{
    struct IndexTableSpec
    {
        const char *sbs_name;        // 历史名称记录（描述符时代键名，仅注释性）
        const char *buffer_name;     // buffer_reference 类型名（加 Ref 后缀）
        const char *var_name;        // （旧描述符对象名——BDA 化后无对象，仅保留作注释性记录）
        const char *resolve_func;    // 为空则仅生成 buffer 声明
        const char *element_type;    // 行元素 GLSL 类型（默认 uint；Arena 地址表为 uint64_t）
        const char *root_addr_field; // pc_root 字段名（如 addr_l2w_index）
    };

    // mesh 阶段只需 l2w_index；材质地址表在 FS 消费（见 BuildFSIndexTableDecls）。
    // A6-2a：行表本体 buffer_reference——地址经 pc_root.addr_l2w_index 下发；
    // 恒发射（去契约门）。sbs_name 字段为历史名称记录（SBS_LocalToWorldIndex
    // 常量已随 A6-2b-b2 删除——行表不再有描述符名，仅 buffer_reference 类型名）。
    const IndexTableSpec kMeshIndexTableSpecs[] = {
        { "l2w_index", "LocalToWorldIndex", "l2w_index",
          "ResolveTransformID", "uint", "addr_l2w_index" },
    };

    void AppendIndexTableDecl(
        std::string &out,
        const IndexTableSpec &spec)
    {
        // A6-2a：无 set 无 binding——类型声明 + resolve 函数经 pc_root 地址解引用。
        // l2w_index 恒发射（与 l2w_ssbo 恒注入一致；无 L2W 材质不消费 ResolveTransformID，
        // 多余 unused 声明无害）。不再经契约 GetSSBO 门（契约 L2W/L2WIndex 条目已删）。
        out += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer ";
        out += spec.buffer_name;
        out += "Ref { ";
        out += spec.element_type;
        out += " values[]; };\n";

        if (spec.resolve_func)
        {
            // 行表写单列（values[iid]），每个值直接对应一个材质数据行。
            out += spec.element_type;
            out += " ";
            out += spec.resolve_func;
            out += "(uint iid) { return ";
            out += spec.element_type;
            out += "(";
            out += spec.buffer_name;
            out += "Ref(pc_root.";
            out += spec.root_addr_field;
            out += ").values[iid]); }\n";
        }
    }
}//namespace

static std::string BuildMeshIndexTableDecls()
{
    std::string out;

    // A6-2a：恒发射——不再查契约（GetSSBO）。与 l2w_ssbo 的无条件注入对应：
    // mesh 侧需要 ResolveTransformID/l2w_index 的材质恒可得，无需契约中间层。
    for (const IndexTableSpec &spec : kMeshIndexTableSpecs)
        AppendIndexTableDecl(out, spec);

    return out;
}

    // FS 侧 RootAddresses push constant block 发射——与 mesh 侧（MeshShaderHeaderGen）
    // 遍历同一 HGL_ROOT_ADDRESSES_FIELD_LIST，布局逐字段一致（不同 stage 访问同一
    // push constant range 必须布局一致）。FS 消费 pc_root.addr_mtl_data_addrs。
    void EmitFSRootAddressesPushConstant(std::string &out)
    {
        out += "layout(push_constant) uniform RootAddresses\n";
        out += "{\n";
        for (uint32 field_index = 0;
             field_index < kRootAddressesFieldCount;
             ++field_index)
        {
            out += "    ";
            out += kRootAddressesFieldGLSLTypes[field_index];
            out += " ";
            out += kRootAddressesFieldNames[field_index];
            out += ";\n";
        }
        out += "} pc_root;\n";
        out += "\n";
    }

static std::string BuildFSIndexTableDecls(const bool fs_has_runtime_rows)
{
    std::string out;

    if (!fs_has_runtime_rows)
        return out;

    // 逐字段遍历 HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST（唯一真源在
    // hgl/graph/ShaderBufferSources.h）——新增/改名/调序字段只改那份列表，
    // 本函数与 CPU struct、布局断言自动跟随，无手写漂移面。
    out += "struct MaterialInstanceAddresses\n";
    out += "{\n";
    for (uint32 field_index = 0;
         field_index < kMaterialInstanceAddressesFieldCount;
         ++field_index)
    {
        out += "    ";
        out += kMaterialInstanceAddressesFieldGLSLTypes[field_index];
        out += " ";
        out += kMaterialInstanceAddressesFieldNames[field_index];
        out += ";\n";
    }
    out += "};\n";
    out +=
        "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialInstanceAddressesRef\n";
    out += "{\n";
    out += "    MaterialInstanceAddresses values[];\n";
    out += "};\n";

    // D3：阴影接收位（真源 = MaterialShadowFlags 枚举，避免 GLSL 侧硬编码数值）
    out += "#define HGL_MATERIAL_SHADOW_FLAG_NO_RECEIVE ";
    out += std::to_string(
        static_cast<uint32>(kMaterialShadowFlagNoReceive));
    out += "u\n";

    return out;
}

namespace
{
    void AddStageDiagnostic(
        ShaderDocumentDiagnostics &diagnostics,
        const char *code,
        const char *message)
    {
        ShaderDocumentDiagnostic *diagnostic = diagnostics.Create();
        diagnostic->code = code;
        diagnostic->message = message;
    }

    void AppendDocumentBlocks(
        ShaderDocument &out_document,
        const ShaderDocument &fragment,
        const char *stage,
        const char *material)
    {
        for (int i = 0; i < fragment.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = fragment.GetBlock(i);
            ShaderDocumentSource source = block.source;
            source.stage = stage;
            source.material = material ? material : "";
            out_document.Add(block.kind, block.text, source);
        }
    }

}

bool BuildMaterialStageDocument(
    const ShaderDocument &source_document,
    const ShaderStage stage,
    const char *material,
    const MaterialCompileConfig &config,
    const GlobalSSBOType material_private_data,
    ShaderDocument &out_document,
    ShaderDocumentDiagnostics &out_diagnostics)
{
    out_document.Clear();
    out_diagnostics.Clear();

    const char *stage_name = nullptr;
    if (stage == ShaderStage::Mesh)
        stage_name = "mesh";
    else if (stage == ShaderStage::Fragment)
        stage_name = "fragment";
    else
    {
        AddStageDiagnostic(
            out_diagnostics,
            "unsupported-stage",
            "Material stage document requires Mesh or Fragment stage");
        return false;
    }

    ShaderDocument injection;
    if (stage == ShaderStage::Fragment)
    {
        ShaderDocumentSource source;
        source.stage = stage_name;
        source.material = material ? material : "";
        source.logical_name = "MaterialMeshShaderExtension";
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_mesh_shader : require\n",
            source);

        // BDA：pc_root(uint64_t push constant)与 MaterialDataAddressesRef/材质行
        //（buffer_reference）依赖以下扩展——Fragment 侧 pc_root 在 injection 最先
        // 发出，扩展必须先于它（scalar_block_layout 由 GLSLCompiler 编译前全局注入）
        source.logical_name = "MaterialBDAShaderExtension";
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_buffer_reference : require\n",
            source);
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_ARB_gpu_shader_int64 : require\n",
            source);
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n",
            source);

        // pc_root push constant（Fragment 侧）——MTL_ROW 宏与
        // MaterialDataAddressesRef(pc_root.addr_mtl_data_addrs) 引用；
        // 与 mesh 侧声明同一 X 列表，布局逐字段一致
        source.logical_name = "MaterialRootAddressesPC";
        std::string fs_pc_root;
        EmitFSRootAddressesPushConstant(fs_pc_root);
        injection.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(fs_pc_root.c_str()),
            source);

        ShaderDocument compile_defines;
        if (!BuildCompileDefineDocument(config, compile_defines))
            return false;
        AppendDocumentBlocks(
            injection, compile_defines, stage_name, material);

        if (config.material_definition)
        {
            const std::string sampler_macros =
                BuildSamplerMacros(config.material_definition->sampler_names);
            if (!sampler_macros.empty())
            {
                source.logical_name = "MaterialSamplerMacros";
                injection.Add(
                    ShaderDocumentBlockKind::Define,
                    AnsiString(sampler_macros.c_str()),
                    source);
            }
        }
    }

    ShaderDocument resources;
    std::string error;
    if (!BuildMaterialResourceDocument(
            material_private_data,
            config.material_definition,
            resources,
            error))
    {
        AddStageDiagnostic(
            out_diagnostics,
            "material-resource",
            error.c_str());
        return false;
    }

    if (source_document.GetBlockCount() == 0
     || source_document.GetBlock(0).kind != ShaderDocumentBlockKind::Version)
    {
        AddStageDiagnostic(
            out_diagnostics,
            "source-version",
            "Material stage source document must start with a Version block");
        return false;
    }

    struct StageBlockEntry
    {
        ShaderDocumentBlock block{};
        int order = 0;
        size_t sequence = 0;
    };

    std::vector<StageBlockEntry> merged_blocks;
    merged_blocks.reserve(
        static_cast<size_t>(source_document.GetBlockCount())
        + static_cast<size_t>(injection.GetBlockCount())
        + static_cast<size_t>(resources.GetBlockCount()));

    const auto add_block = [&merged_blocks, stage_name, material](
        const ShaderDocumentBlock &block)
    {
        StageBlockEntry entry{};
        entry.block = block;
        entry.block.source.stage = stage_name;
        entry.block.source.material = material ? material : "";
        entry.order = ShaderDocument::GetBlockOrder(entry.block.kind);
        entry.sequence = merged_blocks.size();
        merged_blocks.push_back(entry);
    };

    const ShaderDocumentBlock &version = source_document.GetBlock(0);
    ShaderDocumentSource source = version.source;
    source.stage = stage_name;
    source.material = material ? material : "";
    ShaderDocumentBlock version_block = version;
    version_block.source = source;
    add_block(version_block);

    for (int i = 0; i < injection.GetBlockCount(); ++i)
        add_block(injection.GetBlock(i));

    if (stage == ShaderStage::Fragment)
    {
        for (int i = 0; i < resources.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = resources.GetBlock(i);
            if (std::strcmp(
                    block.source.logical_name.c_str(),
                    "MaterialMeshIndexTables") == 0)
                continue;
            add_block(block);
        }
    }
    else if (stage == ShaderStage::Mesh)
    {
        for (int i = 0; i < resources.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = resources.GetBlock(i);
            if (std::strcmp(
                    block.source.logical_name.c_str(),
                    "MaterialMeshIndexTables") != 0)
                continue;
            add_block(block);
        }
    }

    for (int index = 1; index < source_document.GetBlockCount(); ++index)
    {
        add_block(source_document.GetBlock(index));
    }

    std::sort(
        merged_blocks.begin(),
        merged_blocks.end(),
        [](const StageBlockEntry &lhs, const StageBlockEntry &rhs)
        {
            if (lhs.order != rhs.order)
                return lhs.order < rhs.order;
            return lhs.sequence < rhs.sequence;
        });

    for (const StageBlockEntry &entry : merged_blocks)
        out_document.Add(entry.block.kind, entry.block.text, entry.block.source);
    return true;
}

}  // namespace hgl::graph::mtl
