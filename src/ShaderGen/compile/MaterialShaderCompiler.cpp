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
// 两个阶段都物理固定：`config.fixed_mesh_source` /
// `config.fixed_fragment_source` 原样直通（网格阶段=固定 .mesh 源，片段阶段=
// 领域主着色器），没有占位符、没有代码模块拼装、没有文档合并。
// 此处只负责把它们编译成 SPV + 反射。
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
    const MaterialCompileConfig &config)
{
    // ctx 尚未创建，此处 FailCompile 只负责统一报告（delete nullptr 安全）
    CompileContext c{&input};

    // ── 两个阶段都必须是物理固定源 ────────────────────────────────
    // 生成器与文档层删除后不再有"合并 → 序列化"环节：两份固定源就是最终
    // 编译单元。缺任一份都是硬失败——静默降级只会让网格阶段变成空壳，症状
    // 出现在下游的 FinalizeShaderBuildContext，看不出根因。
    if (!config.fixed_mesh_source || !config.fixed_mesh_source[0])
    {
        c.Fail("mesh stage requires a fixed GLSL source "
               "(ShaderLibrary/mesh/shape/<shape_name>.mesh)");
        return FailCompile(c);
    }
    if (!config.fixed_fragment_source || !config.fixed_fragment_source[0])
    {
        c.Fail("fragment stage requires a fixed GLSL source "
               "(the domain master shader)");
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

    // ── Step 5: 两份固定源直接成为最终阶段文本 ───────────────────
    // 没有文档合并/序列化：文本本身即完整编译单元，再套一层 version/define
    // 前缀会写出重复指令。
    ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
    ShaderCreateInfo *frag = ctx->GetStageShader(ShaderStage::Fragment);
    if (mesh)
        mesh->SetFinalGLSL(std::string(config.fixed_mesh_source));
    if (frag)
        frag->SetFinalGLSL(std::string(config.fixed_fragment_source));


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
// 纯函数、零决策。无生成器终态下仍有消费者的只剩：
// DumpShaderGenGLSL / BuildSamplerMacros / MaterialDefinitionRequiresPayloadRow。
// 文档拼装与序列化（BuildMaterialStageDocument 及其全部子步骤）已随文档层删除。
// ═══════════════════════════════════════════════════════════════════════════

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


}  // namespace hgl::graph::mtl
