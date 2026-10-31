#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/Smart.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/DomainEntrySource.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/StageSPVCompiler.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ResolvedPassProfile.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ScenePassProfileResolver.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include "ShaderProgramBuild/builder/MaterialProgramAssembler.h"
#include "ShaderProgramBuild/compile/ShaderProgramBuildBackendInstall.h"
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/GlslModuleRegistry.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/util/hash/FNV1a.h>

#include <cstring>
#include <vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    bool VerifyPayloadModuleIdentity()
    {
        auto &registry = GetGlslModuleRegistry();
        const char *source_names[] = { "texture_source", "pbr_surface_source" };
        const char *material_ids[] = { "UnlitTexture", "Lit" };
        for (int i = 0; i < 2; ++i)
        {
            MaterialDefinition definition;
            const auto *source = registry.FindByName(source_names[i]);
            if (!source || !TryGetMaterialDefinitionByID(material_ids[i], definition))
                return false;
            GlslModuleDefinition named = *source;
            named.name = "payload_test_identity_not_a_filename";
            if (!registry.Register(named))
                return false;
            definition.material_source_module_name = named.name;

            // 副本的模块名故意既不含 "texture_source" 也不含 "pbr_surface_source"：
            // payload 行需求必须由模块注解（`require Resource MaterialData`）决定，
            // 而不是由名字子串反推。
            bool has_payload = false;
            const bool queried = MaterialDefinitionRequiresPayloadRow(
                definition.material_private_data, &definition, has_payload);
            registry.RemoveByName(named.name);
            if (!queried || has_payload != (i == 1))
            {
                GLogError(u8"Payload inference must use module identity: %s", source_names[i]);
                return false;
            }

            definition.material_source_module_name = "missing_payload_provider";
            if (MaterialDefinitionRequiresPayloadRow(
                    definition.material_private_data, &definition, has_payload))
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
        /// 位置属性格式覆盖。`VK_FORMAT_UNDEFINED` = 沿用 `vec2_position`
        /// 的 V2F/V3F 二选一。
        ///
        /// 需要它是因为「位置格式决定 s1 解码模块」这条契约**不是**
        /// 浮点专属：`VK_FORMAT_R16G16_SINT` 走 `s1_position_vec2i.glsl`
        /// （4B packed int16x2），与 `vec2`（8B）是两个不同 ABI。只用
        /// `vec2_position` 布尔表达不出来，`DrawTriangle` 那条真实路径
        /// （`VF_V2I16` 像素坐标）就一直进不了回归。
        VkFormat position_format_override = VK_FORMAT_UNDEFINED;
        /// 由 recipe 层指定 2D 顶点节点配置（`Make2DNodeConfigOrtho`）。
        ///
        /// 这面旗子守的是阶段 4.4 修过的真实缺陷：`vertex_node_config`
        /// 有四层来源（request 覆盖 > recipe > definition > 默认），而
        /// `MaterialProgramAssembler::ResolveVertexABI` 必须把求解结果**回写**
        /// 到 `plan.vertex_definition`，否则 ABI 对账读 definition 的
        /// `Passthrough3D`（要 3 分量）、mesh 发射读 recipe 的
        /// `PixelToLocal`（要 2 分量），`CheckPositionComponentCount` 硬失败。
        /// 没有「definition 是 3D、recipe 是 2D」这种组合，写回删掉了也
        /// 不会有人报错。
        bool recipe_2d_node_config = false;
    };

    bool ResolveFixtureTemplateRequest(
        const MaterialDefinition &definition,
        const ShaderProgramPurpose purpose,
        const MaterialRecipe *recipe,
        PassProfileRequest &out_request,
        const char *ambient_module_name)
    {
        const MaterialRecipe effective_recipe =
            recipe ? *recipe : MaterialRecipe{};
        return SelectScenePassProfileRequest(
                   definition, purpose, effective_recipe, out_request,
                   ambient_module_name)
            && AppendMaterialPassProfileRoots(definition, out_request);
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

    /// 固定片段阶段的校验。
    ///
    /// 片段阶段不再有 `ShaderDocument`：主着色器是一份完整编译单元，构建器只把
    /// 它的文本原样交给 `ShaderCreateInfo`。因此这里检查的是文本不变量 + 键单源：
    /// `definition_hash` 必须等于**最终 GLSL 字节**的摘要（构建器侧 `plan.fs`
    /// 与编译侧 `config.fixed_fragment_source` 是同一份字节）。
    bool VerifyFixedFragmentStage(
        const char *fixture,
        const ShaderCreateInfo *actual_stage,
        const ShaderStageKey &stage_key)
    {
        if (!actual_stage)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=fragment is "
                "absent from its production ShaderBuildContext",
                fixture);
            return false;
        }

        const auto &actual = actual_stage->GetFinalGLSL();
        const uint64 actual_hash = HashFinalShaderSource(
            actual.data(), actual.size());
        const int version_count = CountText(
            AnsiString(actual.c_str(), int64(actual.size())), "#version");
        const int main_count = CountText(
            AnsiString(actual.c_str(), int64(actual.size())), "void main(");

        bool valid = true;
        if (actual_hash == 0 || version_count != 1 || main_count != 1
         || actual.size() < 8
         || actual.compare(0, 8, "#version") != 0)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=fragment "
                "final text invariant failed: bytes=%d versions=%d mains=%d",
                fixture, int(actual.size()), version_count, main_count);
            valid = false;
        }

        if (stage_key.definition_hash == 0
         || stage_key.definition_hash != actual_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=fragment "
                "source hash mismatch: actual=%llu stage-key=%llu",
                fixture,
                static_cast<unsigned long long>(actual_hash),
                static_cast<unsigned long long>(stage_key.definition_hash));
            valid = false;
        }
        return valid;
    }

    bool VerifyFixedMeshStage(
        const char *fixture,
        const ShaderCreateInfo *actual_stage,
        const ShaderStageKey &stage_key)
    {
        if (!actual_stage)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s stage=mesh is "
                "absent from its production ShaderBuildContext",
                fixture);
            return false;
        }

        const auto &actual = actual_stage->GetFinalGLSL();
        const uint64 actual_hash = HashFinalShaderSource(
            actual.data(), actual.size());
        if (actual_hash == 0
         || stage_key.definition_hash != actual_hash)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s fixed mesh "
                "source hash mismatch: actual=%llu stage-key=%llu",
                fixture,
                static_cast<unsigned long long>(actual_hash),
                static_cast<unsigned long long>(stage_key.definition_hash));
            return false;
        }
        return true;
    }

    /// 按 fixture 组装完整构建请求并执行一次构建计划。
    /// defer_finalize=true：只算到 ProgramLink（含 stage 键），不跑 SPV。
    /// RunFixture 与 VerifyDeclarationKeyDeterminism 共用，保证两者比对的
    /// 是同一份输入。
    /// definition_mutation：可选，在组请求之前扰动定义副本。stage 键敏感性探针
    /// 用它证明「改这一项 ⇒ 换键」。
    /// out_options_text：可选，回传本 fixture 的 `generated/frag_options.glsl` 文本。
    /// 片段文本已物理固定，选项（provider 选择、alpha 通路、采样档位）全部住在
    /// options 里，因此断言必须落在它上面。
    /// 该夹具是否因「领域缺少本 purpose 的固定主着色器」而无法构建。
    ///
    /// 这不是夹具失败，而是域主着色器表的**覆盖缺口**：`kDomainEntrySources[]` 里只有
    /// `standard_opaque` 声明了 `supports_depth_purpose`，其余 11 个领域没有 depth/shadow
    /// 主着色器。缺口必须在 `[SKIP]` 里显式留痕，不能当成通过——静默跳过会让
    /// 「领域深度能力」这个真实回归永久隐形。补齐主着色器后这里的触发条件应当
    /// 变得不可达（见 plan.md 阶段六）。
    bool FixtureNeedsMissingDomainMaster(
        const Fixture &fixture, AnsiString *out_reason)
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(fixture.material_id, definition))
            return false;

        DomainEntrySource probe;
        if (ResolveDomainEntrySource(
                definition.domain, fixture.purpose, fixture.alpha_test, probe,
                definition.surface_module_name))
            return false;

        if (out_reason)
        {
            *out_reason =
                AnsiString("no fixed domain master for domain=")
                + GetMaterialDomainName(definition.domain)
                + " purpose=" + AnsiString::numberOf(
                      static_cast<int>(fixture.purpose));
        }
        return true;
    }

    ShaderBuildContext *BuildFixtureContext(
        const Fixture &fixture,
        MaterialProgramBuildCapture *capture,
        MaterialDefinition *out_definition = nullptr,
        PassProfileID *out_profile_id = nullptr,
        void (*definition_mutation)(MaterialDefinition &) = nullptr,
        AnsiString *out_options_text = nullptr,
        AnsiString *out_skip_reason = nullptr,
        const char *fixed_mesh_source = nullptr)
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
            const VkFormat position_format =
                fixture.position_format_override != VK_FORMAT_UNDEFINED
                    ? fixture.position_format_override
                    : (fixture.vec2_position ? VF_V2F : VF_V3F);
            if (!geometry.Add(
                    VertexSemantic::Position,
                    position_format,
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
        if (fixture.recipe_2d_node_config)
            request.recipe.vertex_node_config = Make2DNodeConfigOrtho(false);
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
        if (out_profile_id)
            *out_profile_id = request.render_template_request.profile_id;

        // 片段阶段物理固定：构建器不再发射片段，请求必须自带最终文本。
        // 后端内部也是这么做的（`DomainShaderBuildBackend::CreateBuildContext`），
        // 这里手工复现两步是因为本工具要 `defer_finalize`——只要文档与 stage 键，
        // 不碰 SPV，因此不能借后端（后端拿到上下文就立刻把片段编成 SPV）。
        // 先判「该领域有没有本 purpose 的固定主着色器」，缺口交给 `[SKIP]` 留痕。
        if (out_skip_reason
         && FixtureNeedsMissingDomainMaster(fixture, out_skip_reason))
            return nullptr;

        FixedFragmentSource fixed;
        if (!ResolveFixedFragmentSource(definition, request, fixed))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s cannot resolve "
                "fixed fragment source",
                fixture.name);
            return nullptr;
        }
        FixedStageInputs fixed_stage;
        fixed_stage.fragment_source = fixed.source.c_str();
        fixed_stage.fragment_interface_hash = fixed.interface_hash;
        fixed_stage.fragment_options_digest = fixed.options_digest;
        fixed_stage.mesh_source = fixed_mesh_source;

        // 环境光 provider 的选择已经从片元文本搬到 `frag_options.glsl`（主着色器只
        // `#include "generated/frag_options.glsl"`，provider 的 `#include` 由发射器
        // 写在里面）。因此断言必须落在 options 文本上，落在 `GetFinalGLSL()`
        // 上会永远为假。这里顺势一次发射，调用方拿到的就是真正喂给编译的那份。
        if (out_options_text)
        {
            if (!EmitDomainOptionsForDefinition(
                    definition, request, definition.domain,
                    fixture.purpose, *out_options_text))
            {
                GLogError(
                    "[ShaderDocumentProductionRegression] fixture=%s cannot emit "
                    "domain options",
                    fixture.name);
                return nullptr;
            }
        }

        return AssembleMaterialProgram(
            nullptr, request, definition, capture, fixed_stage);
    }

    bool VerifyFixedMeshSourcePassThrough(const Fixture &fixture)
    {
        AutoDelete<ShaderBuildContext> generated(
            BuildFixtureContext(fixture, nullptr));
        if (!generated || !generated->HasProgramLink())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s cannot build "
                "baseline mesh source",
                fixture.name);
            return false;
        }

        const ShaderCreateInfo *generated_mesh =
            generated->GetStageShader(ShaderStage::Mesh);
        if (!generated_mesh || generated_mesh->GetFinalGLSL().empty())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s has no baseline "
                "mesh GLSL",
                fixture.name);
            return false;
        }

        AutoDelete<ShaderBuildContext> fixed(
            BuildFixtureContext(
                fixture, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                generated_mesh->GetFinalGLSL().c_str()));
        if (!fixed || !fixed->HasProgramLink())
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s fixed mesh "
                "pass-through build failed",
                fixture.name);
            return false;
        }

        const ShaderCreateInfo *fixed_mesh =
            fixed->GetStageShader(ShaderStage::Mesh);
        if (!fixed_mesh)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s fixed mesh "
                "stage missing",
                fixture.name);
            return false;
        }

        const bool source_matches =
            std::strcmp(
                generated_mesh->GetFinalGLSL().c_str(),
                fixed_mesh->GetFinalGLSL().c_str()) == 0;
        const uint64 fixed_source_hash = HashFinalShaderSource(
            fixed_mesh->GetFinalGLSL().data(),
            fixed_mesh->GetFinalGLSL().size());
        const bool key_matches_final_source =
            fixed->GetProgramLink().mesh_stage.definition_hash
                == fixed_source_hash;
        if (!source_matches || !key_matches_final_source)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s fixed mesh "
                "source mismatch: source=%d (%llu/%llu bytes) "
                "key_matches_final_source=%d",
                fixture.name,
                source_matches ? 1 : 0,
                static_cast<unsigned long long>(
                    generated_mesh->GetFinalGLSL().size()),
                static_cast<unsigned long long>(
                    fixed_mesh->GetFinalGLSL().size()),
                key_matches_final_source ? 1 : 0);
            return false;
        }

        GLogInfo(
            "[ShaderDocumentProductionRegression] fixture=%s fixed mesh GLSL "
            "pass-through preserves source and hashes final text into stage key",
            fixture.name);
        return true;
    }

    /// `--dump-source <dir>`：把每个 fixture 两个阶段的**最终 GLSL**（注入完成、
    /// glslang 实际吃掉的那份文本）落盘为 `<dir>/<fixture>.<mesh|frag>`。
    ///
    /// 这是"废除 ShaderGen"的等价性证据来源：物理固定主着色器必须编译出与这些
    /// 文本等价的 SPIR-V 接口与反射。`shadergen` 调试通道只能落**源文档**（含尚未
    /// 注入的扩展、pc_root push constant、采样器宏），既不可直接编译也无法对拍。
    const char *g_dump_source_dir = nullptr;

    /// 因「该领域尚无该 purpose 的固定主着色器」而跳过的夹具数。
    /// 非零 = 域主着色器覆盖缺口，必须在收尾日志里显式呈现。
    int g_skipped_fixture_count = 0;

    bool WriteDumpSourceFile(
        const char *fixture,
        const char *extension,
        const char *text,
        const int64 length)
    {
        AnsiString file_name;
        file_name += fixture;
        file_name += ".";
        file_name += extension;

        const hgl::filesystem::Path path =
            hgl::filesystem::Path(ToOSString(g_dump_source_dir))
            / ToOSString(file_name.c_str());

        hgl::io::FileOutputStream *stream =
            hgl::io::CreateFileOutputStream(path.ToOSString());
        if (!stream)
        {
            GLogError(
                "[ShaderDocumentProductionRegression] cannot open source dump %s",
                file_name.c_str());
            return false;
        }
        const bool written = stream->Write(text, length) == length;
        delete stream;
        if (!written)
            GLogError(
                "[ShaderDocumentProductionRegression] failed to write source dump %s",
                file_name.c_str());
        return written;
    }

    bool RunFixture(const Fixture &fixture)
    {
        MaterialProgramBuildCapture capture{};
        MaterialDefinition definition{};
        PassProfileID profile_id = PassProfileID::Unknown;
        AnsiString options_text;
        AnsiString skip_reason;
        AutoDelete<ShaderBuildContext> context(
            BuildFixtureContext(
                fixture, &capture, &definition, &profile_id, nullptr,
                &options_text, &skip_reason));
        if (!context && skip_reason.Length())
        {
            // 覆盖缺口：显式留痕并计数，绝不当作通过。
            GLogWarning(
                "[SKIP] fixture=%s %s",
                fixture.name, skip_reason.c_str());
            ++g_skipped_fixture_count;
            return true;
        }
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
        // Sky 的 surface 选择已经从「文档里合成」变成「主着色器里写死」：domain
        // master 自带 `#include "material/sky_cube_surface.glsl"`，不再由定义挑。
        // 因此这里不再有「文档是否 include 了所选 surface」这回事 —— 该不变量由
        // 门禁的 `DH.*` 域等价与 `verify-domain-master-shaders.ps1` 覆盖。
        // 环境光 provider 仍然可选，但它现在写在 `frag_options.glsl` 里；片段文本已
        // 物理固定，options 才是 provider 的唯一入口。
        if (fixture.ambient_module_name && fixture.ambient_module_name[0])
        {
            const char *name = fixture.ambient_module_name;
            const auto *source = GetGlslModuleRegistry().FindFileByName(name);
            if (!source
             || CountText(options_text,
                    (AnsiString("#include \"") + source->include_path + "\"")
                        .c_str()) != 1)
            {
                GLogError(
                    u8"frag_options.glsl does not include the ambient provider: %s",
                    name);
                return false;
            }
        }
        // 两个阶段都已物理固定，网格阶段不再有"生成器回退"分支：这里只做
        // 固定源的不变量与键单源校验。
        bool valid = VerifyFixedMeshStage(
            fixture.name,
            context->GetStageShader(ShaderStage::Mesh),
            link.mesh_stage);
        valid = VerifyFixedFragmentStage(
            fixture.name,
            context->GetStageShader(ShaderStage::Fragment),
            link.fragment_stage) && valid;

        const ShaderProgramArtifactMetadata &metadata =
            context->GetProgramArtifactMetadata();
        const auto *mesh = context->GetStageShader(ShaderStage::Mesh);
        const auto *fragment = context->GetStageShader(ShaderStage::Fragment);
        GLogInfo(
            "[ShaderDocumentProductionRegression] fixture=%s mesh_shape=%s source=fixed",
            fixture.name,
            AnsiString::numberOf(capture.mesh_source_shape_key).c_str());
        if (g_dump_source_dir)
        {
            const auto dump_stage =
                [&fixture](const ShaderCreateInfo &stage, const char *extension)
            {
                return WriteDumpSourceFile(
                    fixture.name, extension,
                    stage.GetFinalGLSL().data(),
                    int64(stage.GetFinalGLSL().size()));
            };
            if (!dump_stage(*mesh, "mesh") || !dump_stage(*fragment, "frag"))
                valid = false;
            const AnsiString shape_key =
                AnsiString::numberOf(capture.mesh_source_shape_key);
            if (!WriteDumpSourceFile(
                    fixture.name, "mesh-key",
                    shape_key.c_str(), shape_key.Length()))
                valid = false;
        }
        hgl::hash::FNV1aHasher64 source_hasher;
        source_hasher << HashFinalShaderSource(
            mesh->GetFinalGLSL().data(), mesh->GetFinalGLSL().size())
                      << HashFinalShaderSource(
            fragment->GetFinalGLSL().data(), fragment->GetFinalGLSL().size());
        if (metadata.program_key_digest != link.BuildKey().GetDigest()
         || metadata.generated_source_digest != uint64(source_hasher))
        {
            GLogError(
                "[ShaderDocumentProductionRegression] fixture=%s program key "
                "mismatch: program=%llu/%llu source=%llu/%llu",
                fixture.name,
                static_cast<unsigned long long>(metadata.program_key_digest),
                static_cast<unsigned long long>(link.BuildKey().GetDigest()),
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
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.shader_artifact_store = &store;
            if (!ResolveFixtureTemplateRequest(
                    definition, request.shader_program_purpose,
                    &request.recipe, request.render_template_request, nullptr))
                return nullptr;

            // 走**领域后端**而不是直接 `AssembleMaterialProgram`：片段阶段是物理固定的
            // 主着色器，编译它需要「ShaderLibrary + generated/frag_options.glsl 目录」两
            // 个 include 根，而那是插件里的进程级配置，只有后端在编译那一刻设得对。
            // 这里手写一份根管理等于把后端逻辑抄到工具里，迟早漂移。
            InstallDomainShaderBuildBackend();
            ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
            if (!backend)
                return nullptr;
            ShaderBuildContext *context =
                backend->CreateBuildContext(nullptr, definition, request);
            if (!context)
                return nullptr;
            if (!backend->FinalizeBuildContext(context)
             || !context->HasProgramLink())
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
        int built = 0;
        for (int i = 0; i < count; ++i)
        {
            const Fixture &fixture = fixtures[i];

            if (FixtureNeedsMissingDomainMaster(fixture, nullptr))
                continue;

            ++built;

            MaterialProgramBuildCapture capture_a{};
            AutoDelete<ShaderBuildContext> a(
                BuildFixtureContext(fixture, &capture_a));
            MaterialProgramBuildCapture capture_b{};
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
            built);
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

    // MaterialDefinition 里由 emitter 直接写进 GLSL 的注入项。
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
        if (FixtureNeedsMissingDomainMaster(fixture, nullptr))
        {
            GLogWarning(
                "[SKIP] fixture=%s definition sensitivity (no fixed domain "
                "master for this purpose)",
                fixture.name);
            return true;
        }

        MaterialProgramBuildCapture baseline_capture{};
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

            MaterialProgramBuildCapture capture{};
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
            && VerifyDefinitionSensitivity(fixture);
    }

    AnsiString FormatStageKey(const ShaderStageKey &key)
    {
        return AnsiString("stage=")
             + AnsiString::numberOf(static_cast<uint32>(key.stage))
             + " digest=" + AnsiString::numberOf(key.GetDigest())
             + " definition=" + AnsiString::numberOf(key.definition_hash)
             + " compile_inputs=" + AnsiString::numberOf(key.compile_inputs_hash)
             + " interface=" + AnsiString::numberOf(key.interface_hash)
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
    /// 换代、缓存是否全失效」只看 definition/compile_inputs/interface。
    bool DumpDeclarationKeys(
        const Fixture *fixtures, const int count, const char *path)
    {
        AnsiString text;
        for (int i = 0; i < count; ++i)
        {
            if (FixtureNeedsMissingDomainMaster(fixtures[i], nullptr))
                continue;

            MaterialProgramBuildCapture capture{};
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

    /// `--dump-source <dir>`：见 RunFixture 上方的定义。
    struct Options
    {
        bool full = false;
        const char *dump_keys_path = nullptr;
        const char *dump_source_dir = nullptr;
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
            if (std::strcmp(argv[i], "--dump-source") == 0)
            {
                if (i + 1 >= argc)
                    return false;
                out.dump_source_dir = argv[++i];
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
            "ShaderDocumentProductionRegression [--smoke|--full] "
            "[--dump-keys <path>] [--dump-source <dir>]");
        return 2;
    }
    const bool full = options.full;
    g_dump_source_dir = options.dump_source_dir;

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
        // 错误棋盘格（读标记色行）：验证带 `require Resource MaterialData` 的
        // 回退材质能正常出 SPV（它是所有材质错误的兜底渲染路径）。
        { "error-checkerboard-3d-forward", "builtin/error_checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, false, false },
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
        // 蒙版阴影/depth：HGL_COVERAGE_ONLY + HGL_ALPHA_* 同时成立——FS 只求
        // EvalAlpha 并 discard，不发 EvalSurface。此前 fixture 表只有非 alpha 的
        // depth/shadow 用例，这条路径（ShadowCasterMasked）在端到端回归里完全没有
        // 覆盖，ShadowCaster 模板一旦写错不会被任何用例发现。
        { "checkerboard-2d-alpha-depth", "builtin/checkerboard_2d",
          ShaderProgramPurpose::DepthOnly, true, true, false, true },
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
        { "checkerboard-3d-normal-alpha-shadow", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ShadowDepth, true, false, true, true },
        { "checkerboard-3d-normal-alpha", "builtin/checkerboard_3d",
          ShaderProgramPurpose::ForwardColor, true, false, true, true },
        { "lit-forward", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        // PBR 域的 alpha 前向 + 蒙版阴影。这两条此前只有 Checker2D/Checker3D
        // 覆盖，而 PBR 走的是不同的材质源（pbr_surface_source）+ NTB 组合。
        // 删除 ShaderGen 后 domain/standard_opaque/*.frag 需要它们作为等价基线。
        { "lit-forward-alpha", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, true },
        { "lit-shadow-alpha", "Lit", ShaderProgramPurpose::ShadowDepth,
          true, false, false, true },
        { "lit-shadow", "Lit", ShaderProgramPurpose::ShadowDepth,
          true, false, false, false },
        { "text-gpu-charquad", "builtin/text_gpu",
          ShaderProgramPurpose::ForwardColor, false, false, false, false },
        { "sky-minimal", "SkyMinimal", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "sky-cube", "SkyCube", ShaderProgramPurpose::ForwardColor,
          true, false, false, false },
        { "lit-ibl", "Lit", ShaderProgramPurpose::ForwardColor,
          true, false, false, false, "indirect_ibl" },
        // DrawTriangle 的真实形状：`VertexColor` 域 + `VF_V2I16` 像素坐标
        // + recipe 层 2D 节点配置（`Make2DNodeConfigOrtho`）。
        //
        // 它守三件事：
        //   ① `R16G16_SINT` 必须选到 `vertex/s1_position_vec2i.glsl`（4B
        //      packed int16x2），不是浮点 vec2 的 8B 版本；
        //   ② `ResolveVertexABI` 的 `vertex_node_config` 回写（阶段 4.4
        //      缺陷）——去掉回写后 ABI 读 definition 的 `Passthrough3D`
        //      （3 分量）与 s1 的 2 分量冲突，构建直接失败；
        //   ③ 该形状必须在固定 mesh 源目录里有一份 `.mesh`，否则
        //      ShaderGen 删除后这个域没有 mesh 阶段。
        { "vert-color-2d-int16", "VertexColor",
          ShaderProgramPurpose::ForwardColor, true, false, false, false,
          nullptr, VF_V2I16, true }
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

    if (!VerifyFixedMeshSourcePassThrough(fixtures[0]))
        return 11;

    if (!VerifyDeclarationKeyDeterminism(fixtures, fixture_count))
        return 8;

    if (!VerifyDeclarationKeySensitivity(fixtures[0]))
        return 10;

    GLogInfo(
        "[ShaderDocumentProductionRegression] %s production fixtures passed",
        full ? "full" : "smoke");
    if (g_skipped_fixture_count > 0)
    {
        // 不做「跳过也算过」的默认：缺口必须被收尾日志打断视线。
        GLogWarning(
            "[ShaderDocumentProductionRegression] %s: %d fixture(s) skipped for "
            "missing fixed domain masters (depth/shadow coverage gap, see "
            "plan.md 阶段六)",
            full ? "full" : "smoke", g_skipped_fixture_count);
    }
    return 0;
}
