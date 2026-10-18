/// GenericMaterialBuilder.cpp — phase-split generic material compilation.
///
/// This file is the behavior-preserving decomposition of the former
/// BuildGenericMaterial function in MaterialDefinitionRegistry.cpp. Every
/// statement executes in the original order; intermediate values travel via
/// GenericMaterialBuildPlan. Do not reorder statements here — stage keys and
/// resource contracts depend on the exact hash input sequence.

#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/graph/font/CharQuadConfig.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderCompilerProfileAPI.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include "compile/MaterialShaderEmitter.h"
#include "builder/MaterialResourceManifestBuilder.h"
#include "meshgen/MeshTemplateEmitter.h"
#include "builder/GenericMaterialBuilder.h"

#include <cstring>
#include <vector>
#include <algorithm>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/LoadDataArray.h>
#include <hgl/thread/ThreadMutex.h>
#include <hgl/type/UnorderedMap.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    static AnsiString s_last_build_generic_material_error;

    namespace
    {
        // 设备能力推导：group size ≤ min(max_mesh_work_group_size_x,
        //   floor(max_mesh_output_vertices / 每线程顶点数),
        //   floor(max_mesh_output_primitives / 每线程图元数))。
        // 拒绝生成侧硬编码——设备上限由主程序从物理设备实测后经 profile 传入；
        // profile 为 null 或 limits 未填（0）时退回理想值。
        // VertexPassthrough 采用跨步协作模型（64 线程处理 192 顶点 64 三角形）。
        uint32_t ClampMeshInvocationsByDevice(
            const contract::PhysicalDeviceProfileLite *profile,
            const MeshShaderMode mode,
            const uint32_t ideal) noexcept
        {
            if (!profile)
                return ideal;

            const auto &l = profile->limits;
            const uint32_t verts_per_inv =
                GetMeshModeVerticesPerInvocation(mode);
            const uint32_t prims_per_inv =
                GetMeshModePrimitivesPerInvocation(mode);

            uint32_t cap = l.max_mesh_work_group_size_x;
            if (l.max_mesh_output_vertices > 0)
                cap = std::min(cap, l.max_mesh_output_vertices / verts_per_inv);
            if (l.max_mesh_output_primitives > 0)
                cap = std::min(cap, l.max_mesh_output_primitives / prims_per_inv);

            if (cap == 0)
                return ideal;   // limits 未填（0）= 无约束，用理想值

            uint32_t result = std::min(ideal, cap);
            return result > 0 ? result : ideal;
        }

        bool IsVertexSemanticRequiredForVarying(
            const VertexSemantic semantic,
            const MaterialVertexVaryingConfig &varying) noexcept
        {
            switch (semantic)
            {
            case VertexSemantic::Position:
                return true;
            case VertexSemantic::Normal:
                return varying.emit_world_normal;
            case VertexSemantic::Tangent:
            case VertexSemantic::Bitangent:
                return false;
            case VertexSemantic::TexCoord:
                return varying.emit_uv0;
            case VertexSemantic::Color:
                return varying.emit_vertex_color
                    || varying.emit_vertex_color_from_palette;
            case VertexSemantic::Luminance:
                return varying.emit_luminance;
            case VertexSemantic::TransformID:
                return varying.use_transform_id_attr;
            default:
                return true;
            }
        }

        bool HasVertexSemanticRequirement(
            const MaterialDefinition &definition,
            const VertexSemantic semantic) noexcept
        {
            for (int i = 0;
                 i < definition.vertex_semantic_requirements.GetCount();
                 ++i)
            {
                if (GetVertexSemanticFromShaderCodeModuleSemantic(
                        definition.vertex_semantic_requirements[i].semantic)
                    == semantic)
                    return true;
            }
            return false;
        }

        // Phase 1 — purpose / coverage / varying / stage interface
        // (originally MaterialDefinitionRegistry.cpp:235-305)
        // ═══════════════════════════════════════════════════════════════════
        bool ResolvePurposeAndCoverage(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            plan.purpose = request.shader_program_purpose;
            const RenderTemplateRequest &resolved_request =
                request.render_template_request;
            if (resolved_request.template_id == RenderTemplateID::Unknown)
            {
                GLogError(
                   "[ShaderGen] Material build requires a resolved render template request: name=%s",
                   definition.definition_name.c_str());
                return false;
            }
            if (!BuildMaterialCoverageContract(
                    definition,
                    request.recipe,
                    resolved_request,
                    plan.purpose,
                    plan.coverage))
                return false;

            // 曲面 alpha 是否写进输出：alpha_blend 的 SRC_ALPHA 取输出 alpha
            //（不写进输出 ⇒ blend 空转；见 ComposeInput 注释）。
            {
                const ResolvedMaterialRenderState render_state =
                    ResolveMaterialRenderState(definition, request.recipe);

                plan.alpha_to_output = render_state.pipeline_config.alpha_blend;
            }
            {
                plan.render_template_request_storage = resolved_request;
                plan.render_template_request = &plan.render_template_request_storage;
                RenderTemplateValidationDiagnostic diagnostic{};
                const ShaderCodeModuleRegistry &module_registry =
                   GetShaderCodeModuleRegistry();
                if (!ValidateRenderTemplateRequest(
                       *plan.render_template_request,
                       module_registry,
                       diagnostic)
                 || ((plan.purpose == ShaderProgramPurpose::DepthOnly
                   || plan.purpose == ShaderProgramPurpose::ShadowDepth)
                  && plan.render_template_request->template_id
                      != (plan.coverage.requires_alpha_evaluation
                           ? RenderTemplateID::ShadowCasterMasked
                           : RenderTemplateID::ShadowCasterOpaque)))
            {
                GLogError(
                   "[ShaderGen] Render template request rejected: name=%s "
                   "template=%s validation=%s role=%s module=%s include=%s expected=%s",
                   definition.definition_name.c_str(),
                   GetRenderTemplateName(
                       plan.render_template_request->template_id),
                   GetRenderTemplateValidationErrorName(diagnostic.error),
                   GetShaderModuleSlotRoleName(diagnostic.role),
                   diagnostic.module_name.c_str(),
                   diagnostic.include_path.c_str(),
                   diagnostic.expected_include_path.c_str());
                return false;
            }
            if (!ResolveRenderTemplate(
                   *plan.render_template_request,
                   GetShaderCodeModuleRegistry(),
                   plan.resolved_render_template,
                   diagnostic))
            {
                GLogError(
                   "[ShaderGen] Render template resolution failed: name=%s "
                   "template=%s validation=%s role=%s module=%s include=%s expected=%s",
                   definition.definition_name.c_str(),
                   GetRenderTemplateName(
                       plan.render_template_request->template_id),
                   GetRenderTemplateValidationErrorName(diagnostic.error),
                   GetShaderModuleSlotRoleName(diagnostic.role),
                   diagnostic.module_name.c_str(),
                   diagnostic.include_path.c_str(),
                   diagnostic.expected_include_path.c_str());
                return false;
            }
            plan.render_template_request_storage =
                plan.resolved_render_template.request;
            plan.resolved_template_hash =
                plan.render_template_request_storage.GetHash();
            }
            plan.depth_purpose =
                plan.purpose == ShaderProgramPurpose::DepthOnly
             || plan.purpose == ShaderProgramPurpose::ShadowDepth;

            plan.effective_vertex_varying =
                ResolveMaterialVertexVaryingConfig(
                    definition,
                    plan.purpose,
                    plan.coverage);
            plan.vertex_definition = definition;
            plan.vertex_definition.vertex_varying =
                plan.effective_vertex_varying;
            if (plan.depth_purpose)
            {
                plan.vertex_definition.vertex_semantic_requirements.Clear();
                for (int i = 0;
                     i < definition.vertex_semantic_requirements.GetCount();
                     ++i)
                {
                    const auto &requirement =
                        definition.vertex_semantic_requirements[i];
                    const VertexSemantic semantic =
                        GetVertexSemanticFromShaderCodeModuleSemantic(
                            requirement.semantic);
                    if (IsVertexSemanticRequiredForVarying(
                            semantic, plan.effective_vertex_varying))
                    {
                        plan.vertex_definition.vertex_semantic_requirements.Add(
                            requirement);
                    }
                }
            }

            plan.varying = plan.effective_vertex_varying;

            MaterialStageInterfaceDiagnostic stage_interface_diagnostic{};
            if (!BuildMaterialStageInterface(
                    plan.effective_vertex_varying,
                    plan.stage_interface,
                    stage_interface_diagnostic))
            {
                GLogError(
                    "[ShaderGen] Material stage interface build failed: name=%s error=%s",
                    definition.definition_name.c_str(),
                    GetMaterialStageInterfaceErrorName(
                        stage_interface_diagnostic.error));
                return false;
            }

            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 2 — resolved vertex ABI
        // (originally MaterialDefinitionRegistry.cpp:307-338)
        // ═══════════════════════════════════════════════════════════════════
        bool ResolveVertexABI(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            plan.vertex_node_config =
                ResolveMaterialVertexNodeConfig(definition, request);
            plan.resolved_vertex_input_document.Clear();

            // CharQuad: mesh shader self-declares all SSBOs; no vertex ABI needed.
            if (IsCharQuadMode(definition.mesh_shader_mode))
                return true;

            {
                MaterialResolvedVertexABI resolved_abi;
                if (!BuildResolvedMaterialVertexABI(
                        plan.vertex_definition, request, resolved_abi))
                {
                    GLogError("[ShaderGen] Resolved vertex ABI build failed: name=%s",
                              definition.definition_name.c_str());
                    return false;
                }
                plan.position_format = resolved_abi.position_format;
                if (!resolved_abi.vertex_input_glsl.IsEmpty())
                {
                    ShaderDocumentSource input_source;
                    input_source.stage = "mesh";
                    input_source.module = "vertex-input";
                    input_source.logical_name = "MeshTemplateEmitter.ResolvedInput";
                    plan.resolved_vertex_input_document.Add(
                        ShaderDocumentBlockKind::Module,
                        resolved_abi.vertex_input_glsl,
                        input_source);
                }
            }
            return true;
        }

        /// 资源布局身份 = manifest 内容哈希（契约层删除后的等价式，见 Phase 5 处注释）。
        inline uint64 ComputeResourceLayoutHash(
            const bool depth_purpose, const uint64 manifest_stable_hash) noexcept
        {
            hgl::hash::FNV1aHasher64 empty_entries;
            empty_entries << uint32(0);

            hgl::hash::FNV1aHasher64 h;
            h << (depth_purpose ? uint64(0) : manifest_stable_hash)
              << static_cast<uint64>(empty_entries);
            return h;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 3 — resource manifest
        // (originally MaterialDefinitionRegistry.cpp:339-438)
        // ═══════════════════════════════════════════════════════════════════
        bool BuildResourceContract(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            plan.manifest = ShaderCodeResourceManifest{};
            plan.manifest_definition = definition;
            if (plan.depth_purpose)
                plan.manifest_definition.code_module_requirements.clear();
            const char *provider_root_names[2]{};
            uint32 provider_root_count = 0;
            const ShaderCodeModuleRegistry &module_registry = GetShaderCodeModuleRegistry();
            const bool include_coverage_providers =
                !plan.depth_purpose
             || plan.coverage.requires_alpha_evaluation;
            const ShaderModuleSlotRole provider_roles[] =
            {
                ShaderModuleSlotRole::MaterialSourceProvider,
                ShaderModuleSlotRole::NTBProvider
            };
            for (const ShaderModuleSlotRole role : provider_roles)
            {
                if (!include_coverage_providers
                 || (plan.depth_purpose
                  && role == ShaderModuleSlotRole::NTBProvider))
                    break;
                const RenderTemplateModuleRoot *root =
                    plan.render_template_request->FindModuleRoot(role);
                if (!root)
                    continue;
                const ShaderCodeModuleDefinition *provider =
                    module_registry.FindByName(root->module_name.c_str());
                if (!provider)
                {
                    GLogError("[ShaderGen] Selected provider has no registered metadata: %s",
                              root->module_name.c_str());
                    return false;
                }
                if (provider_root_count < 2)
                    provider_root_names[provider_root_count++] = provider->name;
            }
            if (!resource_manifest_builder::BuildMaterialShaderCodeResourceManifest(
                    plan.manifest_definition, plan.manifest,
                    provider_root_names, provider_root_count, &module_registry))
            {
                GLogError("[ShaderGen] Generic material resource manifest failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }
            const char *invalid_texture_reference = nullptr;
            if (!ValidateShaderCodeResourceManifestTextureReferences(
                    plan.manifest,
                    plan.manifest_definition,
                    &invalid_texture_reference))
            {
                GLogError(
                    "[ShaderGen] Provider texture reference is not declared by material: material=%s texture=%s",
                    definition.definition_name.c_str(),
                    invalid_texture_reference
                        ? invalid_texture_reference
                        : "<invalid>");
                return false;
            }
            const char *missing_sampler_reference = nullptr;
            if (!ValidateShaderCodeResourceManifestSamplerReferences(
                    plan.manifest,
                    plan.manifest_definition,
                    &missing_sampler_reference))
            {
                GLogError(
                    "[ShaderGen] Provider sampler preset is not declared by material: material=%s sampler=%s",
                    definition.definition_name.c_str(),
                    missing_sampler_reference
                        ? missing_sampler_reference
                        : "<invalid>");
                return false;
            }
            if (!plan.manifest.IsValid())
            {
                GLogError("[ShaderGen] Generic material resource contract failed: name=%s error=%s",
                          definition.definition_name.c_str(),
                          GetShaderCodeResourceManifestErrorName(plan.manifest.error));
                return false;
            }
            // 2026-10-11（S1）：原「由 definition 推导描述符条目 → 构建描述符契约」两步已整体删除 ——
            // 推导函数恒返回空（definition 侧不再声明任何描述符），契约与其校验/哈希随之无消费者。
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 4 — vertex + fragment stage sources
        // (originally MaterialDefinitionRegistry.cpp:439-531)
        // ═══════════════════════════════════════════════════════════════════
        bool GenerateStageSources(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            MaterialShaderDocumentCapture *document_capture,
            GenericMaterialBuildPlan &plan)
        {
            if (document_capture)
                document_capture->Clear();

            // Mesh shader 材质：生成 mesh stage。mesh 是唯一顶点路径。
            // 模式选择优先级：definition.mesh_shader_mode > primitive_type 推断
            const bool is_char_quad = IsCharQuadMode(definition.mesh_shader_mode);
            const bool is_lines = !is_char_quad
                && (plan.primitive_type == hgl::graph::PrimitiveType::Lines);

            MeshShaderMode ms_mode;
            uint32_t max_invocations;
            if (is_char_quad)
            {
                ms_mode = MeshShaderMode::CharQuad;
                // CharQuad: 每线程 4 顶点，max_vertices ≤ 256（Vulkan 规范保证下限）
                // 64 × 4 = 256（TEXT_CHARQUAD_MAX_INVOCATIONS 与 CPU dispatch 共享）
                const uint32_t toml_or_ideal = definition.mesh_shader_max_invocations > 0
                    ? std::min(definition.mesh_shader_max_invocations, TEXT_CHARQUAD_MAX_INVOCATIONS)
                    : TEXT_CHARQUAD_MAX_INVOCATIONS;
                max_invocations = ClampMeshInvocationsByDevice(profile, ms_mode, toml_or_ideal);
                // CharQuad 自声明所有 SSBO，不需要外部顶点输入/provider
            }
            else if (is_lines)
            {
                ms_mode = MeshShaderMode::LineQuad;
                max_invocations = ClampMeshInvocationsByDevice(
                    profile, ms_mode, kMeshLineQuadMaxInvocations);
            }
            else
            {
                ms_mode = MeshShaderMode::VertexPassthrough;
                max_invocations = ClampMeshInvocationsByDevice(
                    profile, ms_mode, kMeshVertexPassthroughMaxInvocations);
            }

            ShaderDocument &mesh_document = document_capture
                ? document_capture->mesh_source_document
                : plan.mesh_source_document;
            // 直调 mesh 阶段发射器。原先这里经 MeshTemplateComposer 门面转发——
            // 那个类体只有一句"9 实参原样透传"，没有任何编排（对照 fragment 侧的
            // FragmentTemplateComposer 才是真编排器），仅增加一层间接，已删除。
            if (!EmitMeshTemplateDocument(
                    plan.vertex_node_config,
                    plan.varying,
                    plan.position_format,
                    ms_mode,
                    max_invocations,
                    mesh_document,
                    &plan.resolved_vertex_input_document,
                    &plan.stage_interface))
            {
                s_last_build_generic_material_error = "EmitMeshTemplateDocument failed";
                GLogError("[ShaderGen] Generic material mesh document build failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }
            {
                ShaderDocumentDiagnostics diagnostics;
                AnsiString serialized;
                if (!mesh_document.Serialize(serialized, diagnostics))
                {
                    s_last_build_generic_material_error = "mesh_document.Serialize failed";
                    GLogError("[ShaderGen] Generic material mesh document serialization failed: name=%s",
                              definition.definition_name.c_str());
                    for (int i = 0; i < diagnostics.GetCount(); ++i)
                    {
                        const ShaderDocumentDiagnostic &d = *diagnostics[i];
                        GLogError("[ShaderGen] mesh-doc-diagnostic[%d] code=%s message=%s block_index=%d stage=%s logic=%s module=%s path=%s",
                                  i,
                                  d.code.c_str(),
                                  d.message.c_str(),
                                  d.block_index,
                                  d.source.stage.c_str(),
                                  d.source.logical_name.c_str(),
                                  d.source.module.c_str(),
                                  d.source.path.c_str());
                    }
                    return false;
                }
                plan.ms.assign(serialized.c_str(), serialized.Length());
            }

            FragmentTemplateComposer composer;
            ShaderDocument &fragment_document = document_capture
                ? document_capture->fragment_document
                : plan.fragment_source_document;
            ShaderDocumentDiagnostics fragment_diagnostics;
            ShaderDocument code_module_document;
            if (!BuildCodeModuleDocument(
                    plan.manifest.IsValid() ? &plan.manifest : nullptr,
                    "fragment",
                    definition.definition_name.c_str(),
                    code_module_document))
            {
                s_last_build_generic_material_error = "BuildCodeModuleDocument failed";
                GLogError("[ShaderGen] Generic material code module document build failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }
            FragmentTemplateComposer::ComposeInput compose_input{};
            compose_input.resolved_template =
                plan.resolved_render_template.IsValid()
                    ? &plan.resolved_render_template : nullptr;
            compose_input.alpha_test =
                plan.coverage.mode == MaterialCoverageMode::AlphaTest
             || plan.coverage.mode
                    == MaterialCoverageMode::AlphaTestDither;
            compose_input.alpha_cutoff = plan.coverage.alpha_cutoff;
            compose_input.dither =
                plan.coverage.mode == MaterialCoverageMode::Dither
             || plan.coverage.mode
                    == MaterialCoverageMode::AlphaTestDither;
            compose_input.alpha_to_output = plan.alpha_to_output;
            compose_input.fragment_inputs = &plan.stage_interface;
            compose_input.output_contract = &plan.output_contract;
            compose_input.coverage_contract = &plan.coverage;
            compose_input.code_module_document = &code_module_document;
            compose_input.texture_declarations = &definition.texture_declarations;
            if (!composer.Compose(
                    compose_input, fragment_document, fragment_diagnostics))
            {
                s_last_build_generic_material_error = "composer.Compose failed: ";
                for (int i = 0; i < fragment_diagnostics.GetCount(); ++i)
                {
                    s_last_build_generic_material_error += fragment_diagnostics[i]->message.c_str();
                }
                GLogError("[ShaderGen] Generic material fragment document build failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }
            {
                AnsiString serialized;
                if (!fragment_document.Serialize(serialized, fragment_diagnostics))
                {
                    s_last_build_generic_material_error = "fragment_document.Serialize failed: ";
                    for (int i = 0; i < fragment_diagnostics.GetCount(); ++i)
                    {
                        const ShaderDocumentDiagnostic &d = *fragment_diagnostics[i];
                        s_last_build_generic_material_error += "[";
                        s_last_build_generic_material_error += d.code.c_str();
                        s_last_build_generic_material_error += "] ";
                        s_last_build_generic_material_error += d.message.c_str();
                        s_last_build_generic_material_error += "; ";
                    }
                    GLogError("[ShaderGen] Generic material fragment document serialization failed: name=%s",
                              definition.definition_name.c_str());
                    for (int i = 0; i < fragment_diagnostics.GetCount(); ++i)
                    {
                        const ShaderDocumentDiagnostic &d = *fragment_diagnostics[i];
                        GLogError("[ShaderGen] fragment-doc-diagnostic[%d] code=%s message=%s block_index=%d stage=%s logic=%s module=%s path=%s",
                                  i,
                                  d.code.c_str(),
                                  d.message.c_str(),
                                  d.block_index,
                                  d.source.stage.c_str(),
                                  d.source.logical_name.c_str(),
                                  d.source.module.c_str(),
                                  d.source.path.c_str());
                    }
                    return false;
                }
                plan.fs.assign(serialized.c_str(), serialized.Length());
            }

            if (document_capture)
            {
                document_capture->CaptureSourceDocuments(
                    mesh_document, fragment_document);
            }
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // ShaderLibrary 模块依赖内容哈希
        //
        // 最终文档（plan.ms / plan.fs）对 ShaderLibrary 里的 GLSL 模块只保留
        // `#include "path"` 指令，真正的展开发生在编译期（GLSLCompiler 按
        // 唯一的 ShaderLibrary 根目录解析并拼接源文件）。
        // 因此若只把文档正文纳入哈希，修改被 include 的模块 .glsl 不会改变
        // stage key，磁盘上旧的 SPV 产物会被永久命中复用（曾导致长时间静默
        // 运行陈旧阴影代码）。这里按同样的 include 搜索顺序递归遍历依赖闭包，
        // 把每个模块文件的正文并入哈希，使模块内容变化必然产生新的 stage key。
        // ═══════════════════════════════════════════════════════════════════

        /// 以「长度前缀 + 原始字节」方式并入哈希，保证拼接边界可区分
        void HashShaderLibraryText(
            hgl::hash::FNV1aHasher64 &hasher, const char *text, const size_t length)
        {
            hasher << static_cast<uint64>(length);

            if (text && length > 0)
                hasher.AppendBytes(text, length);
        }

        /// 与 GLSLCompiler 使用相同的唯一 include root
        OSString ResolveShaderLibraryModulePath(const AnsiString &include_path)
        {
            if (include_path.IsEmpty())
                return OSString();

            return mtl::ResolveShaderLibraryModulePath(
                GetShaderIncludeRoot(), include_path);
        }

        /// include 闭包的两类消费者：stage key 的哈希、声明/文本闭包对齐用的
        /// include 路径集。两者必须走同一次解析，否则"进哈希的文件"与"用来
        /// 比对的路径集"会各自漂移。
        ///
        /// 比对口径用 include 路径（"ao/identity.glsl"）而不是模块名
        /// （"identity_ao"）——两者并不总是同构，按名字归一会产生假差异。
        struct GLSLIncludeClosureSink
        {
            hgl::hash::FNV1aHasher64 *hasher = nullptr;
            std::vector<AnsiString> *include_paths = nullptr;
        };

        bool ContainsAnsiString(
            const std::vector<AnsiString> &list, const AnsiString &value)
        {
            for (const AnsiString &item : list)
                if (std::strcmp(item.c_str(), value.c_str()) == 0)
                    return true;
            return false;
        }

        /// 闭包集合按字典序归一，保证比对与诊断输出稳定
        void SortAnsiStrings(std::vector<AnsiString> &list)
        {
            std::sort(
                list.begin(), list.end(),
                [](const AnsiString &lhs, const AnsiString &rhs)
                {
                    return std::strcmp(lhs.c_str(), rhs.c_str()) < 0;
                });
        }

        void CollectIncludePath(
            GLSLIncludeClosureSink &sink, const AnsiString &include_path)
        {
            if (!sink.include_paths || include_path.IsEmpty())
                return;
            if (ContainsAnsiString(*sink.include_paths, include_path))
                return;
            sink.include_paths->push_back(include_path);
        }

        /// manifest 里是模块名，转成注册表登记的 include_path 后再进集合；
        /// metadata-only 模块（无文件）退回模块名，比对时会表现为"多余"——安全方向。
        void CollectModuleName(
            GLSLIncludeClosureSink &sink, const char *module_name)
        {
            if (!module_name || !module_name[0])
                return;

            const ShaderCodeModuleFileData *file =
                GetShaderCodeModuleRegistry().FindFileByName(module_name);
            if (file && !file->include_path.IsEmpty())
            {
                CollectIncludePath(sink, file->include_path);
                return;
            }

            CollectIncludePath(sink, AnsiString(module_name));
        }

        /// 递归收集文档里的 `#include "..."`，把闭包内每个模块的正文并入哈希，
        /// 同时（可选）把闭包覆盖到的模块名收集进 sink。
        void AccumulateGLSLIncludeClosure(
            GLSLIncludeClosureSink &sink,
            const char *text, const size_t length, const int depth)
        {
            if (!text || length == 0 || depth > 24)
                return;

            size_t line_begin = 0;

            while (line_begin < length)
            {
                size_t line_end = line_begin;
                while (line_end < length && text[line_end] != '\n')
                    ++line_end;

                size_t cursor = line_begin;
                while (cursor < line_end
                    && (text[cursor] == ' ' || text[cursor] == '\t'))
                    ++cursor;

                if (line_end - cursor > 9
                 && std::strncmp(text + cursor, "#include", 8) == 0)
                {
                    cursor += 8;

                    while (cursor < line_end
                        && (text[cursor] == ' ' || text[cursor] == '\t'))
                        ++cursor;

                    if (cursor < line_end && (text[cursor] == '"' || text[cursor] == '<'))
                    {
                        const char closing = text[cursor] == '"' ? '"' : '>';
                        const size_t name_begin = ++cursor;
                        while (cursor < line_end && text[cursor] != closing)
                            ++cursor;

                        const AnsiString include_path(
                            text + name_begin,
                            static_cast<int>(cursor - name_begin));

                        CollectIncludePath(sink, include_path);

                        if (sink.hasher)
                            HashShaderLibraryText(
                                *sink.hasher,
                                include_path.c_str(),
                                static_cast<size_t>(include_path.Length()));

                        const OSString module_path =
                            ResolveShaderLibraryModulePath(include_path);

                        if (module_path.IsEmpty())
                        {
                            if (sink.hasher)
                                HashShaderLibraryText(
                                    *sink.hasher, "<module-missing>", 16);
                        }
                        else
                        {
                            const auto module_bytes =
                                hgl::LoadFileToDataArray<uint8>(module_path);
                            const char *module_text =
                                reinterpret_cast<const char *>(module_bytes.data());

                            if (sink.hasher)
                                HashShaderLibraryText(
                                    *sink.hasher, module_text, module_bytes.size());
                            AccumulateGLSLIncludeClosure(
                                sink, module_text, module_bytes.size(), depth + 1);
                        }
                    }
                }

                line_begin = line_end + 1;
            }
        }

        /// 计算一份最终文档的 ShaderLibrary 依赖闭包哈希（同一文档只算一次）
        uint64 ComputeShaderLibraryDependencyHash(const char *document, const size_t length)
        {
            if (!document || length == 0)
                return 0;

            static hgl::ThreadMutex dependency_hash_mutex;
            static hgl::UnorderedMap<uint64, uint64> dependency_hash_cache;

            const OSString root = GetShaderIncludeRoot();
            hgl::hash::FNV1aHasher64 key_hasher;
            key_hasher << HashFinalShaderSource(document, length);
            key_hasher.AppendBytes(root.c_str(), root.Length() * sizeof(os_char));
            const uint64 document_hash = key_hasher;

            {
                hgl::ThreadMutexLock lock(&dependency_hash_mutex);

                uint64 cached_hash = 0;
                if (dependency_hash_cache.Get(document_hash, cached_hash))
                    return cached_hash;
            }

            hgl::hash::FNV1aHasher64 hasher;
            const char policy[] = "ShaderLibraryRootOnly.v1";
            hasher.AppendBytes(policy, sizeof(policy) - 1);
            GLSLIncludeClosureSink sink;
            sink.hasher = &hasher;
            AccumulateGLSLIncludeClosure(sink, document, length, 0);
            const uint64 dependency_hash = hasher;

            {
                hgl::ThreadMutexLock lock(&dependency_hash_mutex);
                dependency_hash_cache.Add(document_hash, dependency_hash);
            }

            return dependency_hash;
        }

        /// 文本闭包 → include 路径集合（与声明闭包同一口径）
        void CollectShaderLibraryIncludePaths(
            const char *document,
            const size_t length,
            std::vector<AnsiString> &out_include_paths)
        {
            if (!document || length == 0)
                return;

            GLSLIncludeClosureSink sink;
            sink.include_paths = &out_include_paths;
            AccumulateGLSLIncludeClosure(sink, document, length, 0);
        }

        /// 模板骨架里硬编码的 `#include` 基座。
        ///
        /// 这些 include 写在 .tmpl 里，不属于任何模块的 dependencies，因此
        /// plan.manifest 覆盖不到（manifest 只展开模块依赖图）。声明闭包必须
        /// 把它们连同传递闭包显式纳入，否则改 descriptor_macros /
        /// surface_interface / alpha_compositor / scene_ubo / forward_flat 中
        /// 任何一个都不会让声明键失效。
        bool AccumulateTemplateBaseClosure(
            GLSLIncludeClosureSink &sink, const char *template_source_file)
        {
            AnsiString relative("fragment/");
            relative += template_source_file;

            const OSString template_path =
                ResolveShaderLibraryModulePath(relative);
            if (template_path.IsEmpty())
                return false;

            const auto template_bytes =
                hgl::LoadFileToDataArray<uint8>(template_path);
            const char *template_text =
                reinterpret_cast<const char *>(template_bytes.data());

            if (sink.hasher)
                HashShaderLibraryText(
                    *sink.hasher, template_text, template_bytes.size());

            AccumulateGLSLIncludeClosure(
                sink, template_text, template_bytes.size(), 0);
            return true;
        }

        /// ShadowCaster 没有 .glsl.tmpl（FragmentTemplateComposer::ComposeShadow
        /// 走 C++ 手写发射），其硬编码基座 include 在这里镜像给出。与 ComposeShadow
        /// 的发射列表不一致时，声明/文本闭包对齐诊断会直接报出来。
        const char *const kShadowCasterBaseIncludes[] =
        {
            "common/surface_interface.glsl",
            "ubo/scene_ubo.glsl",
            "common/alpha_compositor.glsl",
        };

        /// 非 alpha 的深度/阴影变体只发空 main，**一个 #include 都没有**；
        /// alpha 变体才发上面三个基座 + SurfaceProvider / MaterialSourceProvider。
        bool AccumulateShadowCasterBaseClosure(
            GLSLIncludeClosureSink &sink, const bool requires_alpha)
        {
            if (!requires_alpha)
                return true;

            for (const char *include_path : kShadowCasterBaseIncludes)
            {
                const AnsiString path(include_path);
                CollectIncludePath(sink, path);

                if (sink.hasher)
                    HashShaderLibraryText(
                        *sink.hasher,
                        include_path,
                        static_cast<size_t>(std::strlen(include_path)));

                const OSString module_path =
                    ResolveShaderLibraryModulePath(path);
                if (module_path.IsEmpty())
                    return false;

                const auto module_bytes =
                    hgl::LoadFileToDataArray<uint8>(module_path);
                const char *module_text =
                    reinterpret_cast<const char *>(module_bytes.data());

                if (sink.hasher)
                    HashShaderLibraryText(
                        *sink.hasher, module_text, module_bytes.size());
                AccumulateGLSLIncludeClosure(
                    sink, module_text, module_bytes.size(), 1);
            }
            return true;
        }

        /// 声明闭包 vs 文本闭包对齐诊断。
        ///
        /// 文本闭包（ComputeShaderLibraryDependencyHash 扫出来的文件集）是当前
        /// stage key 的事实覆盖；声明闭包是换成声明键之后会覆盖的集合。只有
        /// 前者 ⊆ 后者时，声明键才可能成为权威键——缺一个模块就意味着那个模块
        /// 的改动不会让键失效，会直接命中过期的 SPV。
        ///
        /// 每类差异只报一次，避免逐材质刷屏。
        void ReportDeclarationClosureDivergence(
            const MaterialDefinition &definition,
            const GenericMaterialBuildPlan &plan,
            std::vector<AnsiString> &text_include_paths)
        {
            if (!plan.declaration_key_ready)
                return;

            SortAnsiStrings(text_include_paths);

            AnsiString missing;
            AnsiString extra;

            for (const AnsiString &text_path : text_include_paths)
            {
                if (ContainsAnsiString(plan.declaration_include_paths, text_path))
                    continue;
                if (!missing.IsEmpty())
                    missing += " ";
                missing += text_path.c_str();
            }

            for (const AnsiString &declaration_path
                    : plan.declaration_include_paths)
            {
                if (ContainsAnsiString(text_include_paths, declaration_path))
                    continue;
                if (!extra.IsEmpty())
                    extra += " ";
                extra += declaration_path.c_str();
            }

            if (missing.IsEmpty() && extra.IsEmpty())
                return;

            hgl::hash::FNV1aHasher64 signature_hasher;
            signature_hasher << static_cast<uint64>(missing.Length());
            if (!missing.IsEmpty())
                signature_hasher.AppendBytes(
                    missing.c_str(), static_cast<size_t>(missing.Length()));
            signature_hasher << static_cast<uint64>(extra.Length());
            if (!extra.IsEmpty())
                signature_hasher.AppendBytes(
                    extra.c_str(), static_cast<size_t>(extra.Length()));
            const uint64 signature = signature_hasher;

            static hgl::ThreadMutex report_mutex;
            static hgl::UnorderedMap<uint64, uint8> reported;

            {
                hgl::ThreadMutexLock lock(&report_mutex);

                uint8 seen = 0;
                if (reported.Get(signature, seen))
                    return;

                reported.Add(signature, 1);
            }

            GLogWarning(
                "[ShaderGen] 声明闭包与文本闭包不一致（声明键尚不可用）: "
                "material=%s 声明缺少=[%s] 声明多余=[%s]",
                definition.definition_name.c_str(),
                missing.IsEmpty() ? "" : missing.c_str(),
                extra.IsEmpty() ? "" : extra.c_str());
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 3.5 — 声明键（emit 之前）
        //
        // 目的：把 stage key 的模块图分量从"已生成的 GLSL 文本"换到"生成前的
        // 声明闭包"。这是运行时与 ShaderGen 解耦的前提——运行时不该为了拿到
        // 缓存键而先跑一遍 GLSL 生成。
        //
        // 本阶段当前**不改变任何 stage key**：FinalizeProgramLink 仍按文本派生，
        // 逐位不变 ⇒ 现有 SPV 缓存全部继续命中。这里只做两件事：
        //   1. 把输出契约前移到 emit 之前（它只依赖 plan.purpose，纯声明）；
        //   2. 在 emit 之前算出模块闭包，供 FinalizeProgramLink 与文本闭包对齐。
        //
        // 闭包来源（全部在 emit 之前就绪）：
        //   a. plan.manifest.code_module_names
        //      —— AddModule 的 DFS 已展开传递依赖（依赖先于依赖者入表），
        //         且 BuildStableHash 已把每个模块的 glsl_code 哈希折进 stable_hash；
        //   b. 渲染模板骨架里硬编码的 `#include` 基座
        //      —— 见 AccumulateTemplateBaseClosure / AccumulateShadowCasterBaseClosure。
        //
        // 只有对齐干净之后，才能把 stage key 真正换到这个源上。
        // ═══════════════════════════════════════════════════════════════════
        bool BuildDeclarationKey(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            plan.declaration_key_ready = false;
            plan.declaration_include_paths.clear();
            plan.declaration_module_graph_hash = 0;
            plan.declaration_template_base_hash = 0;

            MaterialOutputContractDiagnostic output_diagnostic{};
            if (!BuildMaterialOutputContract(
                    plan.purpose,
                    plan.output_contract,
                    output_diagnostic))
            {
                s_last_build_generic_material_error =
                    "BuildMaterialOutputContract failed";
                GLogError(
                    "[ShaderGen] Material output contract build failed: name=%s error=%s",
                    definition.definition_name.c_str(),
                    GetMaterialOutputContractErrorName(
                        output_diagnostic.error));
                return false;
            }

            if (!plan.render_template_request)
                return true;

            const RenderTemplateID template_id =
                plan.render_template_request->template_id;

            hgl::hash::FNV1aHasher64 base_hasher;
            GLSLIncludeClosureSink base_sink;
            base_sink.hasher = &base_hasher;
            base_sink.include_paths = &plan.declaration_include_paths;

            const char *template_file =
                GetRenderTemplateFragmentSourceFile(template_id);

            // 以下失败一律非致命：声明键当前不参与任何 stage key，只要算不出来
            // 就置 declaration_key_ready=false 让诊断跳过，绝不因此让整个构建失败
            // ——第 1 步的前提是零行为变更。
            if (template_file)
            {
                if (!AccumulateTemplateBaseClosure(base_sink, template_file))
                {
                    GLogWarning(
                        "[ShaderGen] 声明键：模板基座闭包读取失败，跳过: name=%s file=%s",
                        definition.definition_name.c_str(), template_file);
                    return true;
                }
            }
            else if (template_id == RenderTemplateID::ShadowCasterOpaque
                  || template_id == RenderTemplateID::ShadowCasterMasked)
            {
                if (!AccumulateShadowCasterBaseClosure(
                        base_sink, plan.coverage.requires_alpha_evaluation))
                {
                    GLogWarning(
                        "[ShaderGen] 声明键：ShadowCaster 基座闭包读取失败，跳过: name=%s",
                        definition.definition_name.c_str());
                    return true;
                }
            }

            // 模板模块根：composer 通过 ResolvedInclude(slot_role) 把它们逐个
            // #include 进最终文档，而 plan.manifest 只含「材质自身要求 + 两个
            // provider 根」，覆盖不到它们。深度用途走 ComposeShadow：非 alpha
            // 变体一个 include 都不发，alpha 变体才补两个 provider 根。
            const char *declaration_roots[MaxRenderTemplateModuleRoots]{};
            uint32 declaration_root_count = 0;

            if (template_file)
            {
                for (uint32 i = 0;
                     i < plan.render_template_request->module_root_count;
                     ++i)
                {
                    const RenderTemplateModuleRoot &root =
                        plan.render_template_request->module_roots[i];
                    if (root.module_name.IsEmpty())
                        continue;
                    declaration_roots[declaration_root_count++] =
                        root.module_name.c_str();
                }
            }
            else if (plan.coverage.requires_alpha_evaluation)
            {
                const ShaderModuleSlotRole shadow_roles[] =
                {
                    ShaderModuleSlotRole::SurfaceProvider,
                    ShaderModuleSlotRole::MaterialSourceProvider,
                };
                for (const ShaderModuleSlotRole role : shadow_roles)
                {
                    const RenderTemplateModuleRoot *root =
                        plan.render_template_request->FindModuleRoot(role);
                    if (!root || root->module_name.IsEmpty())
                        continue;
                    declaration_roots[declaration_root_count++] =
                        root->module_name.c_str();
                }
            }

            ShaderCodeResourceManifest declaration_manifest{};
            const ShaderCodeModuleRegistry &module_registry =
                GetShaderCodeModuleRegistry();
            if (!resource_manifest_builder::BuildMaterialShaderCodeResourceManifest(
                    plan.manifest_definition,
                    declaration_manifest,
                    declaration_roots,
                    declaration_root_count,
                    &module_registry))
            {
                GLogWarning(
                    "[ShaderGen] 声明键：闭包清单构建失败，跳过: name=%s",
                    definition.definition_name.c_str());
                return true;
            }

            for (uint32 i = 0; i < declaration_manifest.code_module_count; ++i)
            {
                const char *module_name =
                    declaration_manifest.code_module_names[i];
                if (module_name && module_name[0])
                    CollectModuleName(base_sink, AnsiString(module_name));
            }

            // 已知过近似（安全方向，暂不修）：Sky 模板的
            // AmbientLightProvider 根（forward_lighting）在 RenderTemplate.cpp
            // 里声明，但 sky.glsl.tmpl 从不 include 它——它属于 dead module root。
            // 去掉会改 resolved_template_hash ⇒ 失效现有 sky SPV 缓存，留给后续步骤。
            SortAnsiStrings(plan.declaration_include_paths);

            hgl::hash::FNV1aHasher64 module_hasher;
            module_hasher << declaration_manifest.stable_hash
                          << plan.resolved_template_hash
                          << static_cast<uint64>(base_hasher);

            plan.declaration_template_base_hash = base_hasher;
            plan.declaration_module_graph_hash = module_hasher;
            plan.declaration_key_ready = true;
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 5 — compiler input / link spec
        // (originally MaterialDefinitionRegistry.cpp:533-596)
        // ═══════════════════════════════════════════════════════════════════
        bool FinalizeProgramLink(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan,
            MaterialShaderCompilerInput &out_compiler_input,
            MaterialCompileConfig &out_config)
        {
            out_compiler_input = MaterialShaderCompilerInput{
                definition.definition_name.c_str(),
                request.primitive_type
            };
            MaterialCompileConfig &config = out_config;
            config.primitive_type = request.primitive_type;
            config.shader_stage_flag_bits =
                uint32(ShaderStage::MeshFragment);
            plan.contract_definition = definition;
            plan.contract_definition.vertex_varying =
                plan.effective_vertex_varying;
            config.material_definition = &plan.contract_definition;
            config.resource_manifest = plan.manifest.IsValid() ? &plan.manifest : nullptr;
            config.artifact_store = request.shader_artifact_store;
            // 资源布局身份 = manifest 内容哈希（2026-10-11 S1：描述符契约层删除）。
            // 等价式保留：原「契约哈希(空契约, manifest_hash)」= FNV1a(manifest_hash, FNV1a(uint32(0)))
            // —— 空契约的条目哈希是常量 ⇒ 逐位同值，stage key / resource_layout_hash 不变、
            // 不触发 SPV 与管线缓存整体失效。
            const uint64 resource_contract_hash =
                ComputeResourceLayoutHash(
                    plan.depth_purpose, plan.manifest.stable_hash);
            const uint64 vertex_input_hash = request.geometry_vertex_format
                ? request.geometry_vertex_format->GetVertexInputHash() : 0;
            const uint64 compiler_hash =
                contract::GetShaderCompilerProfileHash(profile);
            hgl::hash::FNV1aHasher64 mesh_interface_hasher;
            mesh_interface_hasher << HashFinalShaderSource(
                plan.ms.data(), plan.ms.size())
                                  << vertex_input_hash;
#ifdef _DEBUG
            DumpShaderGenGLSL("mesh", plan.ms);
            DumpShaderGenGLSL("fs", plan.fs);
#endif
            const uint64 mesh_interface_hash = mesh_interface_hasher;
            const uint64 fragment_interface_hash =
                HashFinalShaderSource(plan.fs.data(), plan.fs.size());

            const uint64 mesh_shader_library_hash =
                ComputeShaderLibraryDependencyHash(plan.ms.data(), plan.ms.size());
            const uint64 fragment_shader_library_hash =
                ComputeShaderLibraryDependencyHash(plan.fs.data(), plan.fs.size());

            hgl::hash::FNV1aHasher64 mesh_module_graph_hasher;
            mesh_module_graph_hasher << mesh_shader_library_hash;
            const uint64 mesh_module_graph_hash = mesh_module_graph_hasher;

            // mesh shader 材质：顶点阶段走 mesh stage
            plan.program_link.mesh_stage = BuildFinalShaderStageKey(
                ShaderStage::Mesh,
                plan.ms.data(),
                plan.ms.size(),
                mesh_module_graph_hash,
                mesh_interface_hash,
                resource_contract_hash,
                compiler_hash);
            hgl::hash::FNV1aHasher64 fragment_module_graph_hasher;
            fragment_module_graph_hasher << plan.manifest.stable_hash
                                         << plan.resolved_template_hash
                                         << fragment_shader_library_hash;
            const uint64 fragment_module_graph_hash =
                fragment_module_graph_hasher;
            plan.program_link.fragment_stage = BuildFinalShaderStageKey(
                ShaderStage::Fragment,
                plan.fs.data(),
                plan.fs.size(),
                fragment_module_graph_hash,
                fragment_interface_hash,
                resource_contract_hash,
                compiler_hash);
            plan.program_link.resource_layout_hash =
                resource_contract_hash;
            plan.program_link.vertex_input_hash = vertex_input_hash;
            plan.program_link.render_target_hash =
                GetOutputContractHash(plan.output_contract);
            plan.program_link.compiler_hash = compiler_hash;

            if (plan.declaration_key_ready)
            {
                std::vector<AnsiString> text_include_paths;
                CollectShaderLibraryIncludePaths(
                    plan.fs.data(), plan.fs.size(), text_include_paths);
                ReportDeclarationClosureDivergence(
                    definition, plan, text_include_paths);
            }

            config.program_link = &plan.program_link;
            config.material_private_data = definition.material_private_data;
            config.defer_finalize = request.defer_finalize;
            return true;
        }
    }

    const AnsiString &GetLastBuildGenericMaterialError()
    {
        return s_last_build_generic_material_error;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BuildGenericMaterial — orchestration
    // (originally MaterialDefinitionRegistry.cpp:218-603)
    // ═══════════════════════════════════════════════════════════════════════
    ShaderBuildContext *BuildGenericMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition,
        MaterialShaderDocumentCapture *document_capture)
    {
        s_last_build_generic_material_error.Clear();
        MaterialDefinition resolved_definition = definition;
        if (definition.vertex_normal_mode
                == MaterialVertexNormalMode::OptionalFaceFallback
         && request.geometry_vertex_format
         && request.geometry_vertex_format->Find(VertexSemantic::Normal))
        {
            resolved_definition.vertex_varying.emit_world_normal = true;
            if (!HasVertexSemanticRequirement(
                    resolved_definition, VertexSemantic::Normal))
            {
                resolved_definition.vertex_semantic_requirements.Add(
                    MakeMaterialVertexSemanticRequirement(
                        VertexSemantic::Normal));
            }
            resolved_definition.compile_defines.push_back(
                "HGL_MATERIAL_HAS_VERTEX_NORMAL");
        }

        const bool semantic_contract =
            !resolved_definition.vertex_semantic_requirements.IsEmpty();
        if (!semantic_contract)
        {
            s_last_build_generic_material_error = "semantic_contract invalid";
            GLogError("[ShaderGen] Generic material contract invalid: name=%s semantic_requirements=%d",
                      definition.definition_name.c_str(),
                      resolved_definition.vertex_semantic_requirements.GetCount());
            return nullptr;
        }

        GenericMaterialBuildPlan plan{};

        // Mesh shader 材质：mesh 是唯一顶点路径（不做触发标志/分支；
        // Lines 走 LineQuad，其余走 VertexPassthrough）
        plan.primitive_type = request.primitive_type;

        if (!ResolvePurposeAndCoverage(resolved_definition, request, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "ResolvePurposeAndCoverage failed";
            GLogError("[BuildGenericMaterial] ResolvePurposeAndCoverage failed");
            return nullptr;
        }

        if (!ResolveVertexABI(resolved_definition, request, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "ResolveVertexABI failed";
            GLogError("[BuildGenericMaterial] ResolveVertexABI failed");
            return nullptr;
        }

        if (!BuildResourceContract(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildResourceContract failed";
            GLogError("[BuildGenericMaterial] BuildResourceContract failed");
            return nullptr;
        }

        if (!BuildDeclarationKey(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildDeclarationKey failed";
            GLogError("[BuildGenericMaterial] BuildDeclarationKey failed");
            return nullptr;
        }

        if (!GenerateStageSources(
                profile, resolved_definition, document_capture, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "GenerateStageSources failed";
            GLogError("[BuildGenericMaterial] GenerateStageSources failed");
            return nullptr;
        }

        MaterialShaderCompilerInput compiler_input{};
        MaterialCompileConfig config{};
        if (!FinalizeProgramLink(
                profile, resolved_definition, request, plan,
                                 compiler_input, config))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "FinalizeProgramLink failed";
            GLogError("[BuildGenericMaterial] FinalizeProgramLink failed");
            return nullptr;
        }

        ShaderBuildContext *result = BuildMaterialStageDocuments(
            profile, compiler_input,
            document_capture
                ? document_capture->mesh_source_document
                : plan.mesh_source_document,
            document_capture
                ? document_capture->fragment_document
                : plan.fragment_source_document,
            config, document_capture);
        if (!result)
        {
            s_last_build_generic_material_error = "BuildMaterialStageDocuments failed";
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        }
        return result;
    }
}
