#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/Smart.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/SceneRenderTemplateResolver.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/filesystem/Path.h>
#include "ShaderGen/compile/MaterialShaderEmitter.h"

#include <cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    bool VerifyPayloadModuleIdentity()
    {
        auto &registry = GetShaderCodeModuleRegistry();
        const char *source_names[] = { "texture_source", "pbr_surface_source" };
        const char *material_ids[] = { "UnlitTexture", "Lit" };
        for (int i = 0; i < 2; ++i)
        {
            MaterialDefinition definition;
            const auto *source = registry.FindByName(source_names[i]);
            if (!source || !TryGetMaterialDefinitionByID(material_ids[i], definition))
                return false;
            ShaderCodeModuleDefinition named = *source;
            named.name = "payload_test_identity_not_a_filename";
            if (!registry.Register(named))
                return false;
            definition.material_source_module_name = named.name;
            MaterialCompileConfig config;
            config.material_definition = &definition;
            ShaderDocument input, output;
            input.Add(ShaderDocumentBlockKind::Version, "#version 460\n");
            ShaderDocumentDiagnostics diagnostics;
            const bool built = BuildMaterialStageDocument(
                input, ShaderStage::Fragment, material_ids[i], config,
                definition.material_private_data, output, diagnostics);
            registry.RemoveByName(named.name);
            bool has_payload = false;
            const char *row_name =
                ssbo::GetGlobalSSBORowName(definition.material_private_data);
            for (int block = 0; block < output.GetBlockCount(); ++block)
            {
                if (row_name && output.GetBlock(block).text.FindString(row_name) >= 0)
                    has_payload = true;
            }
            if (!built || has_payload != (i == 1))
            {
                GLogError(u8"Payload inference must use module identity: %s", source_names[i]);
                return false;
            }
            definition.material_source_module_name = "missing_payload_provider";
            if (BuildMaterialStageDocument(
                    input, ShaderStage::Fragment, material_ids[i], config,
                    definition.material_private_data, output, diagnostics)
             || diagnostics.GetCount() == 0)
            {
                GLogError(u8"Unknown payload provider must fail with a diagnostic");
                return false;
            }
        }
        return true;
    }

    struct Fixture
    {
        const char *name;
        const char *material_id;
        ShaderProgramPurpose purpose;
        bool requires_geometry;
        bool vec2_position;
        bool include_normal;
        bool alpha_test = false;
        // 引擎/环境层选定的 ambient provider（`ambient_light_provider` 槽）；
        // nullptr = 模板默认。材质不再声明它。
        const char *ambient_module_name = nullptr;
    };

    bool ResolveFixtureTemplateRequest(
        const MaterialDefinition &definition,
        const ShaderProgramPurpose purpose,
        const MaterialRecipe *recipe,
        RenderTemplateRequest &out_request,
        const char *ambient_module_name)
    {
        const MaterialRecipe effective_recipe =
            recipe ? *recipe : MaterialRecipe{};
        return SelectSceneRenderTemplateRequest(
                   definition, purpose, effective_recipe, out_request,
                   ambient_module_name)
            && AppendMaterialRenderTemplateRoots(definition, out_request);
    }

    const char *GetBlockKindName(const ShaderDocumentBlockKind kind)
    {
        switch (kind)
        {
        case ShaderDocumentBlockKind::Version:  return "version";
        case ShaderDocumentBlockKind::Extension:return "extension";
        case ShaderDocumentBlockKind::Define:   return "define";
        case ShaderDocumentBlockKind::Resource: return "resource";
        case ShaderDocumentBlockKind::Interface:return "interface";
        case ShaderDocumentBlockKind::Module:   return "module";
        case ShaderDocumentBlockKind::Function: return "function";
        case ShaderDocumentBlockKind::MainBody: return "main";
        case ShaderDocumentBlockKind::Raw:      return "raw";
        }
        return "unknown";
    }

    int FindFirstDifference(const AnsiString &lhs, const char *rhs, const int rhs_length)
    {
        const int common_length = lhs.Length() < rhs_length
            ? lhs.Length()
            : rhs_length;
        for (int i = 0; i < common_length; ++i)
            if (lhs[i] != rhs[i])
                return i;
        return lhs.Length() == rhs_length ? -1 : common_length;
    }

    int GetLineNumber(const char *text, const int length, const int offset)
    {
        int line = 1;
        for (int i = 0; i < offset && i < length; ++i)
            if (text[i] == '\n')
                ++line;
        return line;
    }

    AnsiString GetContext(const char *text, const int length, const int offset)
    {
        const int begin = offset > 32 ? offset - 32 : 0;
        const int end = offset + 32 < length ? offset + 32 : length;
        AnsiString result;
        for (int i = begin; i < end; ++i)
        {
            if (text[i] == '\n')
                result += "\\n";
            else if (text[i] == '\r')
                result += "\\r";
            else
                result += AnsiString::charOf(text[i]);
        }
        return result;
    }

    int CountText(const AnsiString &text, const char *needle)
    {
        int count = 0;
        const int needle_length = int(std::strlen(needle));
        for (int offset = 0; offset + needle_length <= text.Length(); ++offset)
        {
            if (std::memcmp(text.c_str() + offset, needle, needle_length) == 0)
                ++count;
        }
        return count;
    }

    const ShaderDocumentBlock *FindBlockAtOffset(
        const ShaderDocument &document, const int offset, int &out_index)
    {
        int begin = 0;
        for (int i = 0; i < document.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = document.GetBlock(i);
            const int end = begin + block.text.Length();
            if (offset < end || (offset == end && i + 1 == document.GetBlockCount()))
            {
                out_index = i;
                return &block;
            }
            begin = end;
        }
        out_index = -1;
        return nullptr;
    }

    bool ReportSerializationMismatch(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document,
        const AnsiString &serialized,
        const char *actual,
        const int actual_length)
    {
        const int offset = FindFirstDifference(serialized, actual, actual_length);
        if (offset < 0)
            return true;

        int block_index = -1;
        const ShaderDocumentBlock *block =
            FindBlockAtOffset(document, offset, block_index);
        const ShaderDocumentSource empty_source{};
        const ShaderDocumentSource &source = block
            ? block->source
            : empty_source;
        const int document_line = GetLineNumber(
            serialized.c_str(), serialized.Length(), offset);
        const int actual_line = GetLineNumber(actual, actual_length, offset);
        const AnsiString document_context = GetContext(
            serialized.c_str(), serialized.Length(), offset);
        const AnsiString actual_context = GetContext(
            actual, actual_length, offset);
        GLogError(
            "[ShaderDocumentProductionRegression] fixture=%s stage=%s GLSL mismatch "
            "offset=%d document(line=%d,length=%d,context=\"%s\") "
            "actual(line=%d,length=%d,context=\"%s\") "
            "block=%d kind=%s source={material=%s,stage=%s,module=%s,path=%s,name=%s}",
            fixture, stage, offset,
            document_line, serialized.Length(), document_context.c_str(),
            actual_line, actual_length, actual_context.c_str(),
            block_index, block ? GetBlockKindName(block->kind) : "end",
            source.material.c_str(), source.stage.c_str(), source.module.c_str(),
            source.path.c_str(), source.logical_name.c_str());
        return false;
    }

    bool ValidateSourceDocument(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document)
    {
        int version_count = 0;
        int main_count = 0;
        bool valid = document.GetBlockCount() > 0;

        for (int i = 0; i < document.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = document.GetBlock(i);
            if (block.kind == ShaderDocumentBlockKind::Version)
                ++version_count;
            if (block.kind == ShaderDocumentBlockKind::MainBody)
                ++main_count;
            if (block.source.stage != stage
             || block.source.logical_name.IsEmpty())
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s stage=%s source "
                    "document block=%d has invalid metadata "
                    "{stage=%s,module=%s,path=%s,name=%s}",
                    fixture, stage, i, block.source.stage.c_str(),
                    block.source.module.c_str(), block.source.path.c_str(),
                    block.source.logical_name.c_str());
                valid = false;
            }
        }

        if (version_count != 1 || main_count != 1
         || document.GetBlock(0).kind != ShaderDocumentBlockKind::Version
         || document.GetBlock(document.GetBlockCount() - 1).kind
                != ShaderDocumentBlockKind::MainBody)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s source "
                "document order invalid: blocks=%d versions=%d mains=%d "
                "first=%s last=%s",
                fixture, stage, document.GetBlockCount(), version_count, main_count,
                document.GetBlockCount() > 0
                    ? GetBlockKindName(document.GetBlock(0).kind) : "none",
                document.GetBlockCount() > 0
                    ? GetBlockKindName(
                        document.GetBlock(document.GetBlockCount() - 1).kind)
                    : "none");
            valid = false;
        }
        return valid;
    }

    bool ValidateFinalDocument(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document,
        AnsiString &out_serialized)
    {
        ShaderDocumentDiagnostics diagnostics;
        if (!document.Serialize(out_serialized, diagnostics))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s final "
                "document serialization failed (diagnostics=%d)",
                fixture, stage, diagnostics.GetCount());
            return false;
        }

        const int version_count = CountText(out_serialized, "#version");
        const int main_count = CountText(out_serialized, "void main(");
        if (version_count != 1 || main_count != 1
         || out_serialized.Left(8).Comp("#version") != 0)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s final "
                "document invariant failed: versions=%d mains=%d first=\"%s\"",
                fixture, stage, version_count, main_count,
                GetContext(out_serialized.c_str(), out_serialized.Length(), 0).c_str());
            return false;
        }
        return true;
    }

    bool VerifyStage(
        const char *fixture,
        const char *stage_name,
        const ShaderDocument &source_document,
        const ShaderDocument &final_document,
        const ShaderCreateInfo *actual_stage,
        const ShaderStageKey &stage_key)
    {
        if (!actual_stage)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s is absent "
                "from its production ShaderBuildContext",
                fixture, stage_name);
            return false;
        }

        bool valid = ValidateSourceDocument(fixture, stage_name, source_document);
        ShaderDocumentDiagnostics diagnostics;
        AnsiString source_serialized;
        if (!source_document.Serialize(source_serialized, diagnostics))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s source "
                "document serialization failed (diagnostics=%d)",
                fixture, stage_name, diagnostics.GetCount());
            return false;
        }
        const uint64 source_document_hash =
            source_document.GetSerializedHash(diagnostics);
        const uint64 source_hash = HashFinalShaderSource(
            source_serialized.c_str(), source_serialized.Length());
        const auto &actual = actual_stage->GetFinalGLSL();

        // stage key 的 definition_hash 现在由「emit 之前的声明源」派生，不再是
        // 最终 GLSL 文本的哈希——这是运行时与 ShaderGen 解耦的前提。
        // 因此这里不再断言两者相等，改为断言它非零、且**确实不等于**文本哈希：
        // 后者是"声明键已生效"的回归守卫，一旦有人把它改回文本派生会立刻失败。
        // 声明键与文本键的语义一致性由 ShaderGen 侧的声明/文本闭包对齐诊断盯着。
        if (source_document_hash == 0 || source_hash == 0
         || stage_key.definition_hash == 0
         || stage_key.definition_hash == source_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s source hash "
                "mismatch: serialized=%llu document=%llu stage-key=%llu",
                fixture, stage_name,
                static_cast<unsigned long long>(source_hash),
                static_cast<unsigned long long>(source_document_hash),
                static_cast<unsigned long long>(stage_key.definition_hash));
            valid = false;
        }

        AnsiString serialized;
        if (!ValidateFinalDocument(
                fixture, stage_name, final_document, serialized))
            return false;

        const uint64 final_document_hash =
            final_document.GetSerializedHash(diagnostics);
        const uint64 final_hash = HashFinalShaderSource(
            serialized.c_str(), serialized.Length());
        const uint64 actual_hash = HashFinalShaderSource(
            actual.data(), actual.size());
        if (final_document_hash == 0 || final_hash == 0
         || final_hash != actual_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s final hash "
                "mismatch: serialized=%llu document=%llu actual=%llu",
                fixture, stage_name,
                static_cast<unsigned long long>(final_hash),
                static_cast<unsigned long long>(final_document_hash),
                static_cast<unsigned long long>(actual_hash));
            valid = false;
        }

        return ReportSerializationMismatch(
                   fixture, stage_name, final_document, serialized,
                   actual.data(), int(actual.size()))
            && valid;
    }

    bool RunFixture(const Fixture &fixture)
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(fixture.material_id, definition))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s cannot resolve material=%s",
                fixture.name, fixture.material_id);
            return false;
        }

        GeometryVertexFormat geometry;
        if (fixture.requires_geometry)
        {
            if (!geometry.Add(
                    VertexSemantic::Position,
                    fixture.vec2_position ? VF_V2F : VF_V3F,
                    0, 0))
                return false;
            const bool requires_ntb =
                definition.ntb_module_name && definition.ntb_module_name[0];
            if (requires_ntb
             && (!geometry.Add(VertexSemantic::TexCoord, VF_V2F, 0, 0)
              || !geometry.Add(VertexSemantic::Normal, VF_V3F, 0, 0)))
                return false;
            if (fixture.include_normal
             && !requires_ntb
             && !geometry.Add(VertexSemantic::Normal, VF_V3F, 0, 0))
                return false;
        }

        MaterialShaderDocumentCapture capture{};
        MaterialDefinitionBuildRequest request{};
        request.recipe.mtl_def_id = definition.definition_id;
        request.recipe.render_state_overrides.has_alpha_test = fixture.alpha_test;
        request.recipe.render_state_overrides.alpha_test = fixture.alpha_test;
        request.recipe.render_state_overrides.has_alpha_cutoff = fixture.alpha_test;
        request.recipe.render_state_overrides.alpha_cutoff = 0.5f;
        request.geometry_vertex_format = fixture.requires_geometry ? &geometry : nullptr;
        request.defer_finalize = true;
        request.shader_program_purpose = fixture.purpose;
        request.ambient_module_name = fixture.ambient_module_name;
        if (!ResolveFixtureTemplateRequest(
                definition, fixture.purpose, &request.recipe,
                request.render_template_request,
                fixture.ambient_module_name))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s cannot resolve template request",
                fixture.name);
            return false;
        }

        AutoDelete<ShaderBuildContext> context(
            CreateMaterialFromDefinition(nullptr, definition, request, &capture));
        if (!context || !context->HasProgramLink()
         || !context->HasProgramArtifactMetadata())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s production material "
                "build failed or did not expose program identity",
                fixture.name);
            return false;
        }

        const ShaderLinkSpec &link = context->GetProgramLink();
        if (request.render_template_request.template_id == RenderTemplateID::Sky)
        {
            const char *name = definition.surface_module_name
                ? definition.surface_module_name : "sky_minimal_surface";
            const auto *source = GetShaderCodeModuleRegistry().FindFileByName(name);
            ShaderDocumentDiagnostics diagnostics;
            AnsiString fragment_text;
            if (!source
             || !capture.fragment_document.Serialize(fragment_text, diagnostics)
             || CountText(fragment_text,
                    (AnsiString("#include \"") + source->include_path + "\"").c_str()) != 1)
            {
                GLogError(u8"Sky document does not include the selected surface: %s", name);
                return false;
            }
        }
        if (fixture.ambient_module_name && fixture.ambient_module_name[0])
        {
            const char *name = fixture.ambient_module_name;
            const auto *source = GetShaderCodeModuleRegistry().FindFileByName(name);
            ShaderDocumentDiagnostics diagnostics;
            AnsiString fragment_text;
            if (!source
             || !capture.fragment_document.Serialize(fragment_text, diagnostics)
             || CountText(fragment_text,
                    (AnsiString("#include \"") + source->include_path + "\"").c_str()) != 1)
            {
                GLogError(u8"Fragment document does not include the ambient provider: %s",
                          name);
                return false;
            }
        }
        bool valid = VerifyStage(
            fixture.name, "mesh", capture.mesh_source_document,
            capture.mesh_final_document,
            context->GetStageShader(ShaderStage::Mesh), link.mesh_stage);
        valid = VerifyStage(
            fixture.name, "fragment", capture.fragment_document,
            capture.fragment_final_document,
            context->GetStageShader(ShaderStage::Fragment), link.fragment_stage)
            && valid;

        const ShaderProgramArtifactMetadata &metadata =
            context->GetProgramArtifactMetadata();
        const auto *mesh = context->GetStageShader(ShaderStage::Mesh);
        const auto *fragment = context->GetStageShader(ShaderStage::Fragment);
        hgl::hash::FNV1aHasher64 source_hasher;
        source_hasher << HashFinalShaderSource(
            mesh->GetFinalGLSL().data(), mesh->GetFinalGLSL().size())
                      << HashFinalShaderSource(
            fragment->GetFinalGLSL().data(), fragment->GetFinalGLSL().size());
        if (metadata.program_key_digest != link.BuildKey().GetDigest()
         || metadata.mesh_stage_digest != link.mesh_stage.GetDigest()
         || metadata.fragment_stage_digest != link.fragment_stage.GetDigest()
         || metadata.generated_source_digest != uint64(source_hasher))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s program/stage key "
                "mismatch: program=%llu/%llu mesh=%llu/%llu fragment=%llu/%llu "
                "source=%llu/%llu",
                fixture.name,
                static_cast<unsigned long long>(metadata.program_key_digest),
                static_cast<unsigned long long>(link.BuildKey().GetDigest()),
                static_cast<unsigned long long>(metadata.mesh_stage_digest),
                static_cast<unsigned long long>(link.mesh_stage.GetDigest()),
                static_cast<unsigned long long>(metadata.fragment_stage_digest),
                static_cast<unsigned long long>(link.fragment_stage.GetDigest()),
                static_cast<unsigned long long>(metadata.generated_source_digest),
                static_cast<unsigned long long>(uint64(source_hasher)));
            valid = false;
        }
        return valid;
    }

    /// 缓存命中 + 命中后校验的端到端验证。
    ///
    /// 其余 fixture 都跑 defer_finalize=true（只验文档，不碰 SPV），因此
    /// FinalizeShaderBuildContext 的命中分支在测试进程内根本不可达，
    /// VerifyGeneratedSourceDigest 也就从未被执行过。这里用真实
    /// ShaderArtifactStore 连做三次构建把那条路径跑通：
    ///   1) 首次：未命中，落盘
    ///   2) 再次：命中（声明键与文本都没变）
    ///   3) 篡改落盘摘要后再建：仍「命中」判定成立，但命中后校验必须
    ///      拒绝它并重编译——这正是声明键欠近似时的兜底行为
    bool VerifyCacheHitPostCheck()
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID("builtin/pure_color", definition))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe cannot "
                "resolve material=builtin/pure_color");
            return false;
        }

        GeometryVertexFormat geometry;
        if (!geometry.Add(VertexSemantic::Position, VF_V3F, 0, 0))
            return false;

        const hgl::filesystem::Path cache_root =
            hgl::filesystem::Path(OS_TEXT("build"))
            / OSString(OS_TEXT("cache-hit-post-check-probe"));

        // 探针不要求空缓存：上一轮留下的产物只会让首个构建直接命中，
        // 而「第二个构建必须命中」「篡改摘要后必须被拒」两条断言
        // 在冷/热缓存下语义一致，因此可重复运行。
        ShaderArtifactStore store(
            cache_root.ToOSString(), ShaderCacheMode::BuildIfMissing);

        auto build_once = [&](ShaderProgramArtifactMetadata &out_metadata)
            -> ShaderBuildContext *
        {
            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = definition.definition_id;
            request.geometry_vertex_format = &geometry;
            request.defer_finalize = false;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.shader_artifact_store = &store;
            if (!ResolveFixtureTemplateRequest(
                    definition, request.shader_program_purpose,
                    &request.recipe, request.render_template_request, nullptr))
                return nullptr;

            ShaderBuildContext *context = CreateMaterialFromDefinition(
                nullptr, definition, request, nullptr);
            if (!context || !context->HasProgramLink())
            {
                delete context;
                return nullptr;
            }
            out_metadata = context->GetProgramArtifactMetadata();
            return context;
        };

        ShaderProgramArtifactMetadata first_metadata{};
        AutoDelete<ShaderBuildContext> first(build_once(first_metadata));
        if (!first)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: first "
                "build failed");
            return false;
        }

        ShaderProgramArtifactMetadata second_metadata{};
        AutoDelete<ShaderBuildContext> second(build_once(second_metadata));
        if (!second)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: second "
                "build failed");
            return false;
        }
        if (!second->UsedCachedProgram())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: second "
                "build must hit the artifact cache (identical declaration and "
                "text)");
            return false;
        }

        // 篡改落盘摘要：命中判定不看它（operator== 已排除），所以仍会「命中」，
        // 命中后校验必须发现文本摘要对不上并拒绝。
        ShaderProgramArtifactMetadata tampered = second_metadata;
        tampered.generated_source_digest ^= 0x5A5A5A5Aull;
        if (!store.SaveProgramMetadata(second->GetProgramLink(), tampered))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: cannot "
                "tamper stored metadata");
            return false;
        }

        ShaderProgramArtifactMetadata third_metadata{};
        AutoDelete<ShaderBuildContext> third(build_once(third_metadata));
        if (!third)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: third "
                "build failed");
            return false;
        }
        if (third->UsedCachedProgram())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: "
                "post-hit digest verification did not reject a tampered "
                "stored digest");
            return false;
        }
        return true;
    }

    bool ParseFull(const int argc, char **argv, bool &out_full)
    {
        if (argc == 1)
            return true;
        if (argc != 2)
            return false;
        if (std::strcmp(argv[1], "--smoke") == 0)
            return true;
        if (std::strcmp(argv[1], "--full") == 0)
        {
            out_full = true;
            return true;
        }
        return false;
    }
}

int main(const int argc, char **argv)
{
    if (!hgl::logger::InitLogger(OS_TEXT("ShaderDocumentProductionRegression")))
        return 3;

    bool full = false;
    if (!ParseFull(argc, argv, full))
    {
        GLogError(
            "[ShaderDocumentProductionRegression] usage: "
            "ShaderDocumentProductionRegression [--smoke|--full]");
        return 2;
    }

    const hgl::filesystem::Path sampler_toml =
        hgl::filesystem::Path(ToOSString(GetShaderLibraryPath()))
        / OSString(OS_TEXT("sampler.toml"));
    if (!SamplerPresetLibrary::Instance().Load(sampler_toml.ToOSString()))
    {
        GLogError(
            "[ShaderDocumentProductionRegression] cannot load sampler presets from %s",
            ToU8String(sampler_toml.ToOSString()).c_str());
        return 4;
    }
    if (!VerifyPayloadModuleIdentity())
        return 5;

    // 缓存命中 + 命中后校验：其余 fixture 走 defer_finalize=true，
    // 永不进入 FinalizeShaderBuildContext 的命中分支，必须单独探针。
    // 探针会真正编译 SPV，因此需要 GLSLCompiler 插件。
    if (!InitShaderCompiler())
    {
        GLogError(
            "[ShaderDocumentProductionRegression] failed to initialize GLSL "
            "compiler for the cache-hit probe");
        return 6;
    }
    if (!VerifyCacheHitPostCheck())
        return 7;

    static const Fixture smoke_fixtures[] =
    {
        { "pure-color-forward", "builtin/pure_color",
          ShaderProgramPurpose::ForwardColor, true, false, false, false },
        { "checkerboard-2d-forward", "builtin/checkerboard_2d",
          ShaderProgramPurpose::ForwardColor, true, true, false, false },
        { "checkerboard-2d-alpha", "builtin/checkerboard_2d",
          ShaderProgramPurpose::ForwardColor, true, true, false, true },
        { "checkerboard-3d-face-forward", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, false, false },
        { "checkerboard-3d-normal-forward", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, true, false },
        { "checkerboard-3d-normal-depth", "builtin/checkerboard_3d",
          ShaderProgramPurpose::DepthOnly, true, false, true, false },
        { "sky-cube", "SkyCube", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "sky-minimal", "SkyMinimal", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        // IBL 环境光照：ambient provider 由**环境层**选（不是材质），
        // 验证它在 smoke 路径(ctest)下确实替换掉默认 indirect_sky_ambient。
        { "lit-ibl", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, false, "indirect_ibl" }
    };
    static const Fixture full_fixtures[] =
    {
        { "pure-color-forward", "builtin/pure_color",
          ShaderProgramPurpose::ForwardColor, true, false, false, false },
        { "pure-color-depth", "builtin/pure_color",
          ShaderProgramPurpose::DepthOnly, true, false, false, false },
        { "pure-color-shadow", "builtin/pure_color",
          ShaderProgramPurpose::ShadowDepth, true, false, false, false },
        { "checkerboard-2d-forward", "builtin/checkerboard_2d",
          ShaderProgramPurpose::ForwardColor, true, true, false, false },
        { "checkerboard-2d-depth", "builtin/checkerboard_2d",
          ShaderProgramPurpose::DepthOnly, true, true, false, false },
        { "checkerboard-2d-alpha", "builtin/checkerboard_2d",
          ShaderProgramPurpose::ForwardColor, true, true, false, true },
        { "checkerboard-3d-face-forward", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, false, false },
        { "checkerboard-3d-face-depth", "builtin/checkerboard_3d",
          ShaderProgramPurpose::DepthOnly, true, false, false, false },
        { "checkerboard-3d-face-shadow", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ShadowDepth, true, false, false, false },
        { "checkerboard-3d-face-alpha", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, false, true },
        { "checkerboard-3d-normal-forward", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, true, false },
        { "checkerboard-3d-normal-depth", "builtin/checkerboard_3d",
          ShaderProgramPurpose::DepthOnly, true, false, true, false },
        { "checkerboard-3d-normal-shadow", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ShadowDepth, true, false, true, false },
        { "checkerboard-3d-normal-alpha", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, true, true },
        { "lit-forward", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "text-gpu-charquad", "builtin/text_gpu",
          ShaderProgramPurpose::ForwardColor, false, false, false, false },
        { "sky-minimal", "SkyMinimal", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "sky-cube", "SkyCube", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "lit-ibl", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, false, "indirect_ibl" }
    };

    const Fixture *fixtures = full ? full_fixtures : smoke_fixtures;
    const int fixture_count = full
        ? int(sizeof(full_fixtures) / sizeof(full_fixtures[0]))
        : int(sizeof(smoke_fixtures) / sizeof(smoke_fixtures[0]));
    for (int i = 0; i < fixture_count; ++i)
    {
        if (!RunFixture(fixtures[i]))
            return 1;
    }

    GLogInfo(
        "[ShaderDocumentProductionRegression] %s production fixtures passed",
        full ? "full" : "smoke");
    return 0;
}
