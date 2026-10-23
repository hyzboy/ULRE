#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/Smart.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/SceneRenderTemplateResolver.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/util/hash/FNV1a.h>
#include "ShaderGen/compile/MaterialShaderEmitter.h"

#include <cstring>
#include <vector>

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
        const uint64 actual_hash = HashFinalShaderSource(
            actual.data(), actual.size());

        // stage key 的 definition_hash 由**发射出来的文档文本**派生：Phase 4 发射
        // 的字节就是键的事实输入，不再有"声明侧影子源"这一层。断言它逐位等于
        // 发射文档的哈希——这是"键单源化"的回归守卫，一旦有人把它改回吃声明
        // 输入会立刻失败。
        if (source_document_hash == 0 || source_hash == 0
         || stage_key.definition_hash == 0
         || stage_key.definition_hash != source_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=%s source hash "
                "mismatch: serialized=%llu document=%llu actual=%llu stage-key=%llu",
                fixture, stage_name,
                static_cast<unsigned long long>(source_hash),
                static_cast<unsigned long long>(source_document_hash),
                static_cast<unsigned long long>(actual_hash),
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

    /// 按 fixture 组装完整构建请求并执行一次编译。
    /// defer_finalize=true：只算到 ProgramLink（含 stage 键），不跑 SPV。
    /// RunFixture 与 VerifyDeclarationKeyDeterminism 共用，保证两者比对的
    /// 是同一份输入。
    /// definition_mutation：可选，在组请求之前扰动定义副本。stage 键敏感性探针
    /// 用它证明「改这一项 ⇒ 换键」。
    ShaderBuildContext *BuildFixtureContext(
        const Fixture &fixture,
        MaterialShaderDocumentCapture *capture,
        MaterialDefinition *out_definition = nullptr,
        RenderTemplateID *out_template_id = nullptr,
        void (*definition_mutation)(MaterialDefinition &) = nullptr)
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(fixture.material_id, definition))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s cannot resolve material=%s",
                fixture.name, fixture.material_id);
            return nullptr;
        }

        if (definition_mutation)
            definition_mutation(definition);

        GeometryVertexFormat geometry;
        if (fixture.requires_geometry)
        {
            if (!geometry.Add(
                    VertexSemantic::Position,
                    fixture.vec2_position ? VF_V2F : VF_V3F,
                    0, 0))
                return nullptr;
            const bool requires_ntb =
                definition.ntb_module_name && definition.ntb_module_name[0];
            if (requires_ntb
             && (!geometry.Add(VertexSemantic::TexCoord, VF_V2F, 0, 0)
              || !geometry.Add(VertexSemantic::Normal, VF_V3F, 0, 0)))
                return nullptr;
            if (fixture.include_normal
             && !requires_ntb
             && !geometry.Add(VertexSemantic::Normal, VF_V3F, 0, 0))
                return nullptr;
        }

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
            return nullptr;
        }

        if (out_definition)
            *out_definition = definition;
        if (out_template_id)
            *out_template_id = request.render_template_request.template_id;

        return CreateMaterialFromDefinition(nullptr, definition, request, capture);
    }

    bool RunFixture(const Fixture &fixture)
    {
        MaterialShaderDocumentCapture capture{};
        MaterialDefinition definition{};
        RenderTemplateID template_id = RenderTemplateID::Unknown;
        AutoDelete<ShaderBuildContext> context(
            BuildFixtureContext(fixture, &capture, &definition, &template_id));
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
        if (template_id == RenderTemplateID::Sky)
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
    /// stage 缓存命中路径需要真实 GLSL 与 SPV。这里用真实
    /// ShaderArtifactStore 连做三次构建验证独立命中：
    ///   1) 首次：未命中，落盘
    ///   2) 再次：命中（stage 键与文本都没变）
    ///   3) 篡改 Mesh 源摘要：Mesh miss 并重新编译，Fragment 仍命中
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
            / OSString(OS_TEXT("cache-stage-independence-probe"));

        // 探针不要求空缓存：上一轮留下的产物只会让首个构建直接命中，
        // 而后续独立命中断言在冷/热缓存下语义一致。
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
        if (!second->UsedCachedMeshStage()
         || !second->UsedCachedFragmentStage())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: second "
                "build must independently hit both stage artifacts");
            return false;
        }

        const ShaderCreateInfo *mesh =
            second->GetStageShader(ShaderStage::Mesh);
        const uint64 mesh_source_digest = ComputeGeneratedStageSourceDigest(
            mesh->GetFinalGLSL().data(), mesh->GetFinalGLSL().size());
        if (!store.SaveStageSPV(
                second->GetProgramLink().mesh_stage,
                mesh->GetSPVData(),
                mesh->GetSPVSize(),
                mesh_source_digest ^ 0x5A5A5A5Aull))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: cannot "
                "tamper Mesh stage source digest");
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
        if (third->UsedCachedMeshStage()
         || !third->UsedCachedFragmentStage())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cache-hit probe: "
                "tampered Mesh digest must miss without invalidating the "
                "Fragment stage artifact");
            return false;
        }
        (void)first_metadata;
        (void)second_metadata;
        (void)third_metadata;
        return true;
    }

    /// stage 键确定性：同一份输入算两次，ProgramLink 必须逐位相同。
    ///
    /// 这是「命中判定只依赖 emit 之前可算的字段」的前提。键若本身不稳定
    /// （典型来源：把含 padding 的结构体整块折进 FNV1a、或折叠顺序依赖
    /// 容器迭代序），缓存就永远不命中，而只跑一次的文档测试看不出来。
    bool VerifyDeclarationKeyDeterminism(
        const Fixture *fixtures, const int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const Fixture &fixture = fixtures[i];

            MaterialShaderDocumentCapture capture_a{};
            AutoDelete<ShaderBuildContext> a(
                BuildFixtureContext(fixture, &capture_a));
            MaterialShaderDocumentCapture capture_b{};
            AutoDelete<ShaderBuildContext> b(
                BuildFixtureContext(fixture, &capture_b));
            if (!a || !a->HasProgramLink() || !b || !b->HasProgramLink())
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s determinism "
                    "probe could not build twice",
                    fixture.name);
                return false;
            }

            const ShaderLinkSpec &link_a = a->GetProgramLink();
            const ShaderLinkSpec &link_b = b->GetProgramLink();
            if (!(link_a.mesh_stage == link_b.mesh_stage)
             || !(link_a.fragment_stage == link_b.fragment_stage)
             || !(link_a.BuildKey() == link_b.BuildKey()))
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s stage key "
                    "is not deterministic: mesh=%llu/%llu fragment=%llu/%llu "
                    "program=%llu/%llu",
                    fixture.name,
                    static_cast<unsigned long long>(link_a.mesh_stage.GetDigest()),
                    static_cast<unsigned long long>(link_b.mesh_stage.GetDigest()),
                    static_cast<unsigned long long>(link_a.fragment_stage.GetDigest()),
                    static_cast<unsigned long long>(link_b.fragment_stage.GetDigest()),
                    static_cast<unsigned long long>(link_a.BuildKey().GetDigest()),
                    static_cast<unsigned long long>(link_b.BuildKey().GetDigest()));
                return false;
            }
        }

        GLogInfo(
            "[ShaderDocumentProductionRegression] stage key determinism: "
            "%d fixtures stable across two builds",
            count);
        return true;
    }

    // ── 接口键敏感性（「不同输入 ⇒ 不同 interface_hash」）──────────────────
    //
    // VerifyDeclarationKeyDeterminism 证明的是「同一输入 ⇒ 同一键」。它的对偶
    // 是「不同输入 ⇒ 不同键」。缺了这一半，一个恒返回常量的假键在确定性探针下
    // 完美通过，却会让所有材质复用同一份 SPV——比不命中更危险，因为它安静。
    //
    // Stage 3 之后，definition/module 两个维度**就是发射文本的哈希**，不存在
    //「加了字段忘了折进哈希」这个失效模式：字段要么改变射出的文本（键自动变），
    // 要么它根本不进 GLSL（那本就不该进键）。真正还靠人手维护的只剩 interface
    // 维度——它刻意不吃文本，只吃契约形状，因此仍需要这份逐字段尺子。
    //
    // 于是本探针守两件事：
    //   ① `affects_interface` 为真的字段扰动后 interface_hash 必须变；
    //   ② 为假的字段扰动后 interface_hash 必须**不变**——反方向泄漏（接口键
    //      偷偷吃实现字段）同样会把本该复用的 SPV 拆成多份。
    //
    // 注入项（compile_defines / sampler_names）不经 ComposeInput，见
    // ComputeInjectedSourceHash。

    /// 一份「所有字段都取非默认值」的 ComposeInput 基线。
    ///
    /// 基线必须全非默认：在默认值上「翻转」可能翻到另一个等价的默认值（等于
    /// 没翻），那样的用例会假通过。ComposeInput 持有指向本结构体成员的裸指针，
    /// 因此本结构体不可拷贝。
    struct ComposeInputFixture
    {
        ResolvedRenderTemplate resolved_template;
        ValueArray<InterStageSemanticContractEntry> fragment_inputs;
        OutputContract output_contract;
        MaterialCoverageContract coverage_contract;
        ShaderDocument code_module_document;
        std::vector<MaterialTextureDeclaration> texture_declarations;
        FragmentTemplateComposer::ComposeInput input;

        ComposeInputFixture() = default;
        ComposeInputFixture(const ComposeInputFixture &) = delete;
        ComposeInputFixture &operator=(const ComposeInputFixture &) = delete;
    };

    void BuildComposeInputFixture(ComposeInputFixture &fixture)
    {
        fixture.resolved_template.stable_hash = 0x1122334455667788ull;

        InterStageSemanticContractEntry varying{};
        varying.semantic = InterStageSemantic::DataIndexID;
        varying.scalar_type = ShaderSemanticScalarType::UnsignedInteger;
        varying.interpolation = InterStageInterpolation::Flat;
        varying.component_count = 1;
        varying.location_width = 1;
        varying.location = 3;
        fixture.fragment_inputs.Add(varying);

        // 必须是一份 ValidateOutputContract 认可的契约：GetOutputContractHash
        // 在契约非法时返回 0，那样任何扰动都「看起来」把键改了（0 → 0 除外），
        // 用例会假通过。
        fixture.output_contract.purpose = ShaderProgramPurpose::ForwardColor;
        fixture.output_contract.depth_only = false;
        ShaderOutputAttachmentContract attachment{};
        attachment.write_semantic_id = 0x1001;
        attachment.value_type = ShaderStageValueType::Vec4;
        attachment.location = 0;
        attachment.location_width = 1;
        attachment.flags = 0;
        fixture.output_contract.attachments.Add(attachment);

        fixture.coverage_contract.mode = MaterialCoverageMode::AlphaTestDither;
        fixture.coverage_contract.alpha_cutoff = 0.25f;
        fixture.coverage_contract.requires_alpha_evaluation = true;
        fixture.coverage_contract.required_semantics = 0x3;
        fixture.coverage_contract.requires_material_data = true;
        fixture.coverage_contract.requires_texture = true;

        fixture.code_module_document.Clear();
        fixture.code_module_document.Add(
            ShaderDocumentBlockKind::Module,
            "vec4 ProbeSensitivity(){ return vec4(1.0); }\n");

        MaterialTextureDeclaration texture{};
        texture.name = "base_color";
        texture.sampler_type = SamplerType::Sampler2D;
        texture.required = true;
        texture.channels = 4;
        fixture.texture_declarations.push_back(texture);

        fixture.input.resolved_template = &fixture.resolved_template;
        fixture.input.alpha_test = true;
        fixture.input.alpha_cutoff = 0.25f;
        fixture.input.dither = true;
        fixture.input.alpha_to_output = true;
        fixture.input.fragment_inputs = &fixture.fragment_inputs;
        fixture.input.output_contract = &fixture.output_contract;
        fixture.input.coverage_contract = &fixture.coverage_contract;
        fixture.input.code_module_document = &fixture.code_module_document;
        fixture.input.texture_declarations = &fixture.texture_declarations;
        fixture.input.shadow_pcf_poisson_taps = 8;
        fixture.input.shadow_normal_offset = 0;
    }

    struct ComposeSensitivityCase
    {
        const char *name;
        bool affects_interface;   // true = 该字段属于 interface_hash 的一部分
        void (*mutate)(ComposeInputFixture &);
    };

    // 逐字段覆盖 ComposeInput 的 12 个成员。新增字段时必须在这里补一行，
    // 否则「加了字段但忘了折进 stage 键」这种事故不会被任何测试拦住。
    const ComposeSensitivityCase compose_sensitivity_cases[] =
    {
        { "resolved_template.stable_hash", false,
          [](ComposeInputFixture &f)
          { f.resolved_template.stable_hash = 0x8877665544332211ull; } },
        { "alpha_test", false,
          [](ComposeInputFixture &f) { f.input.alpha_test = !f.input.alpha_test; } },
        { "alpha_cutoff", false,
          [](ComposeInputFixture &f) { f.input.alpha_cutoff = 0.75f; } },
        { "dither", false,
          [](ComposeInputFixture &f) { f.input.dither = !f.input.dither; } },
        { "alpha_to_output", true,
          [](ComposeInputFixture &f)
          { f.input.alpha_to_output = !f.input.alpha_to_output; } },

        { "fragment_inputs.count", true,
          [](ComposeInputFixture &f)
          {
              InterStageSemanticContractEntry extra{};
              extra.semantic = InterStageSemantic::Color;
              extra.scalar_type = ShaderSemanticScalarType::Float;
              extra.interpolation = InterStageInterpolation::Smooth;
              extra.component_count = 4;
              extra.location_width = 1;
              extra.location = 4;
              f.fragment_inputs.Add(extra);
          } },
        { "fragment_inputs[0].semantic", true,
          [](ComposeInputFixture &f)
          { f.fragment_inputs[0].semantic = InterStageSemantic::StyleID; } },
        { "fragment_inputs[0].scalar_type", true,
          [](ComposeInputFixture &f)
          { f.fragment_inputs[0].scalar_type =
                ShaderSemanticScalarType::SignedInteger; } },
        { "fragment_inputs[0].interpolation", true,
          [](ComposeInputFixture &f)
          { f.fragment_inputs[0].interpolation =
                InterStageInterpolation::NoPerspective; } },
        { "fragment_inputs[0].component_count", true,
          [](ComposeInputFixture &f)
          { f.fragment_inputs[0].component_count = 4; } },
        { "fragment_inputs[0].location_width", true,
          [](ComposeInputFixture &f)
          { f.fragment_inputs[0].location_width = 2; } },
        { "fragment_inputs[0].location", true,
          [](ComposeInputFixture &f) { f.fragment_inputs[0].location = 6; } },

        { "output_contract.purpose", true,
          [](ComposeInputFixture &f)
          { f.output_contract.purpose = ShaderProgramPurpose::ShadowDepth; } },
        { "output_contract.depth_only", true,
          [](ComposeInputFixture &f)
          {
              // depth_only=true 要求附件为空才合法，否则哈希退化成 0。
              f.output_contract.depth_only = true;
              f.output_contract.attachments.Clear();
          } },
        { "output_contract.attachments[0].value_type", true,
          [](ComposeInputFixture &f)
          { f.output_contract.attachments[0].value_type =
                ShaderStageValueType::Vec3; } },
        { "output_contract.attachments[0].location_width", true,
          [](ComposeInputFixture &f)
          { f.output_contract.attachments[0].location_width = 2; } },
        { "output_contract.attachments[0].flags", true,
          [](ComposeInputFixture &f)
          { f.output_contract.attachments[0].flags = 1; } },
        { "output_contract.attachments.count", true,
          [](ComposeInputFixture &f)
          {
              ShaderOutputAttachmentContract extra{};
              extra.write_semantic_id = 0x1002;
              extra.value_type = ShaderStageValueType::Float;
              extra.location = 1;
              extra.location_width = 1;
              f.output_contract.attachments.Add(extra);
          } },

        { "coverage.mode", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.mode = MaterialCoverageMode::Dither; } },
        { "coverage.alpha_cutoff", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.alpha_cutoff = 0.75f; } },
        { "coverage.required_semantics", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.required_semantics = 0x7; } },
        { "coverage.requires_alpha_evaluation", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.requires_alpha_evaluation = false; } },
        { "coverage.requires_material_data", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.requires_material_data = false; } },
        { "coverage.requires_texture", false,
          [](ComposeInputFixture &f)
          { f.coverage_contract.requires_texture = false; } },

        { "texture_declarations[0].name", false,
          [](ComposeInputFixture &f)
          { f.texture_declarations[0].name = "base_color_probe"; } },
        { "texture_declarations[0].sampler_type", false,
          [](ComposeInputFixture &f)
          { f.texture_declarations[0].sampler_type = SamplerType::Sampler3D; } },
        { "texture_declarations[0].required", false,
          [](ComposeInputFixture &f)
          { f.texture_declarations[0].required = false; } },
        { "texture_declarations[0].channels", false,
          [](ComposeInputFixture &f)
          { f.texture_declarations[0].channels = 2; } },
        { "texture_declarations.count", false,
          [](ComposeInputFixture &f)
          {
              MaterialTextureDeclaration extra{};
              extra.name = "normal_map";
              extra.sampler_type = SamplerType::Sampler2D;
              extra.required = false;
              extra.channels = 2;
              f.texture_declarations.push_back(extra);
          } },

        { "shadow_pcf_poisson_taps", false,
          [](ComposeInputFixture &f) { f.input.shadow_pcf_poisson_taps = 32; } },
        { "shadow_normal_offset", false,
          [](ComposeInputFixture &f) { f.input.shadow_normal_offset = 1; } },
    };

    bool VerifyComposeInputSensitivity()
    {
        const int case_count =
            static_cast<int>(sizeof(compose_sensitivity_cases)
                           / sizeof(compose_sensitivity_cases[0]));
        for (int i = 0; i < case_count; ++i)
        {
            const ComposeSensitivityCase &item = compose_sensitivity_cases[i];

            ComposeInputFixture baseline;
            BuildComposeInputFixture(baseline);
            const uint64 baseline_interface =
                FragmentTemplateComposer::GetInterfaceInputHash(baseline.input);

            ComposeInputFixture mutated;
            BuildComposeInputFixture(mutated);
            item.mutate(mutated);
            const uint64 mutated_interface =
                FragmentTemplateComposer::GetInterfaceInputHash(mutated.input);

            const bool interface_changed =
                mutated_interface != baseline_interface;
            if (interface_changed != item.affects_interface)
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] interface key "
                    "sensitivity: %s expected interface_hash %s but it %s",
                    item.name,
                    item.affects_interface ? "to change" : "to stay",
                    interface_changed ? "changed" : "stayed");
                return false;
            }
        }

        GLogInfo(
            "[ShaderDocumentProductionRegression] interface key sensitivity: "
            "%d ComposeInput fields classified correctly",
            case_count);
        return true;
    }

    /// padding 不进键：同一份语义、不同 padding 残留，哈希必须相同。
    ///
    /// 这是「interface_hash 不再吃结构体 padding」这一改动的直接回归。此前
    /// InterStageSemanticContractEntry 走泛型 operator<<，折的是整块 sizeof
    /// 字节，padding 里有什么就吃什么。
    bool VerifyContractEntryPaddingIndependence()
    {
        InterStageSemanticContractEntry clean{};
        clean.semantic = InterStageSemantic::WorldNormal;
        clean.scalar_type = ShaderSemanticScalarType::Float;
        clean.interpolation = InterStageInterpolation::Smooth;
        clean.component_count = 3;
        clean.location_width = 1;
        clean.location = 7;

        // 先把整块填 0xFF，再逐字段赋值：剩下的字节就是 padding。
        InterStageSemanticContractEntry dirty;
        std::memset(&dirty, 0xFF, sizeof(dirty));
        dirty.semantic = clean.semantic;
        dirty.scalar_type = clean.scalar_type;
        dirty.interpolation = clean.interpolation;
        dirty.component_count = clean.component_count;
        dirty.location_width = clean.location_width;
        dirty.location = clean.location;

        hgl::hash::FNV1aHasher64 clean_hasher;
        clean_hasher << clean;
        hgl::hash::FNV1aHasher64 dirty_hasher;
        dirty_hasher << dirty;

        const uint64 clean_hash = clean_hasher;
        const uint64 dirty_hash = dirty_hasher;
        if (clean_hash != dirty_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] contract entry hash "
                "depends on padding: clean=%llu dirty=%llu",
                static_cast<unsigned long long>(clean_hash),
                static_cast<unsigned long long>(dirty_hash));
            return false;
        }

        // 对照：真正改一个语义字段，哈希必须变（否则上面那条断言可能是
        // 「怎么折都不变」的假通过）。
        dirty.location = 9;
        hgl::hash::FNV1aHasher64 changed_hasher;
        changed_hasher << dirty;
        if (static_cast<uint64>(changed_hasher) == clean_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] contract entry hash "
                "ignores the semantic fields");
            return false;
        }
        return true;
    }

    struct DefinitionSensitivityCase
    {
        const char *name;
        bool affects_fragment;
        bool affects_mesh;
        void (*mutate)(MaterialDefinition &);
    };

    // MaterialDefinition 里不经 ComposeInput、由 emitter 直接写进 GLSL 的注入项。
    // 它们只能靠 ComputeInjectedSourceHash 显式折进 stage 键，是 stage 键最脆弱的一段。
    const DefinitionSensitivityCase definition_sensitivity_cases[] =
    {
        // compile_defines → `#define <name> 1`，逐条写进 Define 块。
        { "compile_defines", true, true,
          [](MaterialDefinition &d)
          { d.compile_defines.push_back("HGL_PROBE_SENSITIVITY"); } },
        // sampler_names → `#define <name>Sampler <idx>u`，idx 来自 sampler.toml
        // 的预设顺序，本身不在任何声明源文本里。
        { "sampler_names", true, true,
          [](MaterialDefinition &d)
          { d.sampler_names.push_back("Linear"); } },
    };

    bool VerifyDefinitionSensitivity(const Fixture &fixture)
    {
        MaterialShaderDocumentCapture baseline_capture{};
        AutoDelete<ShaderBuildContext> baseline(
            BuildFixtureContext(fixture, &baseline_capture));
        if (!baseline || !baseline->HasProgramLink())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s definition "
                "sensitivity baseline build failed",
                fixture.name);
            return false;
        }
        const ShaderLinkSpec &baseline_link = baseline->GetProgramLink();

        const int case_count =
            static_cast<int>(sizeof(definition_sensitivity_cases)
                           / sizeof(definition_sensitivity_cases[0]));
        for (int i = 0; i < case_count; ++i)
        {
            const DefinitionSensitivityCase &item =
                definition_sensitivity_cases[i];

            MaterialShaderDocumentCapture capture{};
            AutoDelete<ShaderBuildContext> mutated(BuildFixtureContext(
                fixture, &capture, nullptr, nullptr, item.mutate));
            if (!mutated || !mutated->HasProgramLink())
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s definition "
                    "sensitivity: %s broke the build",
                    fixture.name, item.name);
                return false;
            }

            const ShaderLinkSpec &link = mutated->GetProgramLink();
            const bool fragment_changed =
                !(link.fragment_stage == baseline_link.fragment_stage);
            const bool mesh_changed =
                !(link.mesh_stage == baseline_link.mesh_stage);
            if (fragment_changed != item.affects_fragment
             || mesh_changed != item.affects_mesh)
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s definition "
                    "sensitivity: %s expected fragment/mesh change=%d/%d but "
                    "got %d/%d —— 该注入项没有折进 stage 键",
                    fixture.name, item.name,
                    item.affects_fragment ? 1 : 0,
                    item.affects_mesh ? 1 : 0,
                    fragment_changed ? 1 : 0,
                    mesh_changed ? 1 : 0);
                return false;
            }
        }

        GLogInfo(
            "[ShaderDocumentProductionRegression] definition sensitivity: "
            "%d emitter-injected inputs all move the key",
            case_count);
        return true;
    }

    /// 「不同输入 ⇒ 不同键」：与 VerifyDeclarationKeyDeterminism 互为对偶。
    bool VerifyDeclarationKeySensitivity(const Fixture &fixture)
    {
        return VerifyContractEntryPaddingIndependence()
            && VerifyComposeInputSensitivity()
            && VerifyDefinitionSensitivity(fixture);
    }

    AnsiString FormatStageKey(const ShaderStageKey &key)
    {
        return AnsiString("stage=")
             + AnsiString::numberOf(static_cast<uint32>(key.stage))
             + " digest=" + AnsiString::numberOf(key.GetDigest())
             + " definition=" + AnsiString::numberOf(key.definition_hash)
             + " module_graph=" + AnsiString::numberOf(key.glsl_module_graph_hash)
             + " interface=" + AnsiString::numberOf(key.interface_hash)
             + " resource=" + AnsiString::numberOf(key.resource_hash)
             + " compiler=" + AnsiString::numberOf(key.compiler_hash);
    }

    /// 把每个 fixture 的 stage 键写成可 diff 的文本，用于跨进程/跨机器对照。
    ///
    /// 进程内「同一输入两次算键」已经覆盖了大部分不稳定来源，但覆盖不到一种：
    /// 含 padding 的结构体整块折进 FNV1a 时，同一进程里连续两次调用往往落在
    /// 相同的栈槽、padding 里的残留也相同，于是看起来稳定；换个进程或换台
    /// 机器才会暴露。dump 出来的行可以直接 diff。
    ///
    /// 判读：`compiler=` 来自着色器编译器身份，跨机器可能不同；判断「声明是否
    /// 换代、缓存是否全失效」只看 definition/module_graph/interface/resource。
    bool DumpDeclarationKeys(
        const Fixture *fixtures, const int count, const char *path)
    {
        AnsiString text;
        for (int i = 0; i < count; ++i)
        {
            MaterialShaderDocumentCapture capture{};
            AutoDelete<ShaderBuildContext> context(
                BuildFixtureContext(fixtures[i], &capture));
            if (!context || !context->HasProgramLink())
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s key dump could "
                    "not build",
                    fixtures[i].name);
                return false;
            }

            const ShaderLinkSpec &link = context->GetProgramLink();
            text += fixtures[i].name;
            text += " mesh[";
            text += FormatStageKey(link.mesh_stage);
            text += "] fragment[";
            text += FormatStageKey(link.fragment_stage);
            text += "] program=";
            text += AnsiString::numberOf(link.BuildKey().GetDigest());
            text += "\n";
        }

        hgl::io::FileOutputStream *stream =
            hgl::io::CreateFileOutputStream(ToOSString(path));
        if (!stream)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cannot open key dump %s", path);
            return false;
        }
        const bool written =
            stream->Write(text.c_str(), int64(text.Length())) == int64(text.Length());
        delete stream;

        if (!written)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] failed to write key dump %s", path);
            return false;
        }

        GLogInfo(
            "[ShaderDocumentProductionRegression] wrote %d stage keys to %s",
            count, path);
        return true;
    }

    struct Options
    {
        bool full = false;
        const char *dump_keys_path = nullptr;
    };

    bool ParseOptions(const int argc, char **argv, Options &out)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--smoke") == 0)
                continue;
            if (std::strcmp(argv[i], "--full") == 0)
            {
                out.full = true;
                continue;
            }
            if (std::strcmp(argv[i], "--dump-keys") == 0)
            {
                if (i + 1 >= argc)
                    return false;
                out.dump_keys_path = argv[++i];
                continue;
            }
            return false;
        }
        return true;
    }
}

int main(const int argc, char **argv)
{
    if (!hgl::logger::InitLogger(OS_TEXT("ShaderDocumentProductionRegression")))
        return 3;

    Options options;
    if (!ParseOptions(argc, argv, options))
    {
        GLogError(
            "[ShaderDocumentProductionRegression] usage: "
            "ShaderDocumentProductionRegression [--smoke|--full] [--dump-keys <path>]");
        return 2;
    }
    const bool full = options.full;

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

    // 独立 stage 缓存命中：其余 fixture 走 defer_finalize=true，
    // 不进入 FinalizeShaderBuildContext，必须单独探针。
    // 探针会真正编译 SPV，因此需要 GLSLCompiler 插件。
    // dump-keys 只取 stage 键（不编译 SPV），跳过探针以保持与编译器版本无关。
    if (!options.dump_keys_path)
    {
        if (!InitShaderCompiler())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] failed to initialize GLSL "
                "compiler for the cache-hit probe");
            return 6;
        }
        if (!VerifyCacheHitPostCheck())
            return 7;
    }

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
    if (options.dump_keys_path)
        return DumpDeclarationKeys(fixtures, fixture_count, options.dump_keys_path)
            ? 0 : 9;

    for (int i = 0; i < fixture_count; ++i)
    {
        if (!RunFixture(fixtures[i]))
            return 1;
    }

    if (!VerifyDeclarationKeyDeterminism(fixtures, fixture_count))
        return 8;

    if (!VerifyDeclarationKeySensitivity(fixtures[0]))
        return 10;

    GLogInfo(
        "[ShaderDocumentProductionRegression] %s production fixtures passed",
        full ? "full" : "smoke");
    return 0;
}
