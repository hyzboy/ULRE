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
#include <hgl/mtl/SamplerPreset.h>
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
        // Phase 2.5 — mesh stage 发射模式
        //
        // 模式与 invocation 上限只依赖 profile + definition + primitive，与
        // 生成的 GLSL 无关。提到 Phase 3.5 之前是因为声明键必须折叠它们
        // （它们决定 max_vertices/max_primitives 与部分 #define）。
        // ═══════════════════════════════════════════════════════════════════
        void ResolveMeshStageMode(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            // 模式选择优先级：definition.mesh_shader_mode > primitive_type 推断
            if (IsCharQuadMode(definition.mesh_shader_mode))
            {
                plan.mesh_shader_mode = MeshShaderMode::CharQuad;
                // CharQuad: 每线程 4 顶点，max_vertices ≤ 256（Vulkan 规范保证下限）
                // 64 × 4 = 256（TEXT_CHARQUAD_MAX_INVOCATIONS 与 CPU dispatch 共享）
                const uint32_t toml_or_ideal = definition.mesh_shader_max_invocations > 0
                    ? std::min(definition.mesh_shader_max_invocations, TEXT_CHARQUAD_MAX_INVOCATIONS)
                    : TEXT_CHARQUAD_MAX_INVOCATIONS;
                plan.mesh_max_invocations = ClampMeshInvocationsByDevice(
                    profile, plan.mesh_shader_mode, toml_or_ideal);
                // CharQuad 自声明所有 SSBO，不需要外部顶点输入/provider
            }
            else if (plan.primitive_type == hgl::graph::PrimitiveType::Lines)
            {
                plan.mesh_shader_mode = MeshShaderMode::LineQuad;
                plan.mesh_max_invocations = ClampMeshInvocationsByDevice(
                    profile, plan.mesh_shader_mode, kMeshLineQuadMaxInvocations);
            }
            else
            {
                plan.mesh_shader_mode = MeshShaderMode::VertexPassthrough;
                plan.mesh_max_invocations = ClampMeshInvocationsByDevice(
                    profile, plan.mesh_shader_mode, kMeshVertexPassthroughMaxInvocations);
            }
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
            const MaterialDefinition &definition,
            MaterialShaderDocumentCapture *document_capture,
            GenericMaterialBuildPlan &plan)
        {
            if (document_capture)
                document_capture->Clear();

            const MeshShaderMode ms_mode = plan.mesh_shader_mode;
            const uint32_t max_invocations = plan.mesh_max_invocations;

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
            if (!composer.Compose(
                    plan.compose_input, fragment_document, fragment_diagnostics))
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

        /// MeshTemplateEmitter 发射的硬编码基座 include（镜像
        /// MeshShaderHeaderGen::EmitMeshShaderHeaderResources）。
        ///
        /// 与 fragment 侧 kShadowCasterBaseIncludes 同理：这些 include 不由
        /// 任何模块的 dependencies 声明，manifest 覆盖不到，必须显式纳入
        /// 声明闭包，否则改它们不会让 mesh 声明键失效。
        /// descriptor_macros / scene_ubo 无条件发射；l2w_ssbo 仅在有朝向变换
        /// 需求时发射（谓词与发射器一致，node_cfg 已进定义哈希）。
        const char *const kMeshBaseIncludes[] =
        {
            "common/descriptor_macros.glsl",
            "ubo/scene_ubo.glsl",
        };

        const char *const kMeshL2WInclude = "common/l2w_ssbo.glsl";

        bool AccumulateMeshBaseClosure(
            GLSLIncludeClosureSink &sink,
            const VertexShaderNodeConfig &node_cfg)
        {
            for (const char *include_path : kMeshBaseIncludes)
            {
                CollectIncludePath(sink, AnsiString(include_path));

                if (sink.hasher)
                    HashShaderLibraryText(
                        *sink.hasher,
                        include_path,
                        static_cast<size_t>(std::strlen(include_path)));

                const OSString module_path =
                    ResolveShaderLibraryModulePath(AnsiString(include_path));
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

            const bool needs_l2w =
                node_cfg.orientation == OrientationMode::World
             || node_cfg.orientation == OrientationMode::CameraFacingFree
             || node_cfg.orientation == OrientationMode::CameraFacingAxisY;
            if (!needs_l2w)
                return true;

            CollectIncludePath(sink, AnsiString(kMeshL2WInclude));

            if (sink.hasher)
                HashShaderLibraryText(
                    *sink.hasher,
                    kMeshL2WInclude,
                    static_cast<size_t>(std::strlen(kMeshL2WInclude)));

            const OSString l2w_path =
                ResolveShaderLibraryModulePath(AnsiString(kMeshL2WInclude));
            if (l2w_path.IsEmpty())
                return false;

            const auto l2w_bytes = hgl::LoadFileToDataArray<uint8>(l2w_path);
            const char *l2w_text =
                reinterpret_cast<const char *>(l2w_bytes.data());

            if (sink.hasher)
                HashShaderLibraryText(*sink.hasher, l2w_text, l2w_bytes.size());
            AccumulateGLSLIncludeClosure(sink, l2w_text, l2w_bytes.size(), 1);
            return true;
        }

        /// 声明闭包 vs 文本闭包对齐诊断。
        ///
        /// 文本闭包是切换前 stage key 的事实覆盖；声明闭包是切换后覆盖的集合。
        /// 声明键已在 FinalizeProgramLink 里生效，但本诊断仍保留：它盯的是
        /// 「声明闭包漏了某个实际被 include 的模块」这类会让键失效失败的错误，
        /// 换键之后这个风险不会消失，只会更严重。
        ///
        /// 每类差异只报一次，避免逐材质刷屏。
        void ReportDeclarationClosureDivergence(
            const MaterialDefinition &definition,
            const GenericMaterialBuildPlan::DeclarationKey &key,
            std::vector<AnsiString> &text_include_paths)
        {
            if (!key.ready)
                return;

            SortAnsiStrings(text_include_paths);

            AnsiString missing;
            AnsiString extra;

            for (const AnsiString &text_path : text_include_paths)
            {
                if (ContainsAnsiString(key.include_paths, text_path))
                    continue;
                if (!missing.IsEmpty())
                    missing += " ";
                missing += text_path.c_str();
            }

            for (const AnsiString &declaration_path : key.include_paths)
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
                "[ShaderGen] 声明闭包与文本闭包不一致: "
                "material=%s 声明缺少=[%s] 声明多余=[%s]",
                definition.definition_name.c_str(),
                missing.IsEmpty() ? "" : missing.c_str(),
                extra.IsEmpty() ? "" : extra.c_str());
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 3.5 — 声明键（emit 之前）
        //
        // 目的：让 stage key 完全由"生成前的声明"决定，而不是由"已生成的 GLSL
        // 文本"决定。运行时因此不必为了拿到缓存键先跑一遍代码生成——这是运行时
        // 与 ShaderGen 解耦的前提。
        //
        // 每个 stage 产出四个分量：
        //   template_base_hash —— 骨架 .glsl.tmpl 正文 + 其硬编码 `#include` 基座的
        //                        传递内容闭包（fragment 侧；mesh 骨架无 include）
        //   module_graph_hash  —— 模块闭包的内容哈希
        //                        （fragment: manifest.stable_hash + resolved_template_hash
        //                          + 基座闭包；mesh: 骨架正文 + 三个 stage 模块闭包）
        //   definition_hash    —— 骨架正文 + 基座闭包 + 全部声明输入
        //                        （ComposeInput / vertex ABI / mesh 模式 /
        //                          emitter 注入的 compile_defines 与 sampler 宏），
        //                        语义等价于"生成出来的文本"
        //   interface_hash     —— 对外声明的形状：片元输入 varying + 输出附件 +
        //                        alpha 写输出（fragment）/ 顶点输出 ABI（mesh）
        //
        // definition_hash 与 module_graph_hash 都含基座闭包，重叠是有意的：
        // 两个字段语义不同，重叠只会让失效更保守，不会漏。
        //
        // 所有读取失败都降级为 ready=false 而**不是**让构建失败：FinalizeProgramLink
        // 见到 ready=false 会回退到原来的文本派生路径，保证行为不劣化。
        // ═══════════════════════════════════════════════════════════════════

        // ═══════════════════════════════════════════════════════════════════
        // emitter 注入型宏的声明键折叠
        //
        // MaterialShaderEmitter::BuildMaterialStageDocument 在**每个** stage
        // 文档（mesh 与 fragment）里插入两块宏定义：
        //   compile_defines → "#define <name> 1\n"  (BuildCompileDefineDocument)
        //   sampler_names   → "#define <name>Sampler <idx>u" (BuildSamplerMacros)
        // 它们改的是最终 GLSL 文本，却不属于任何模块依赖、也不进
        // FragmentTemplateComposer 的 ComposeInput ⇒ plan.manifest 与
        // GetDeclarationInputHash 都覆盖不到。不显式纳入声明键，"只改了宏"
        // 就会命中过期 SPV。
        //
        // 折叠的是**发射器实际会写出的内容**，不是字段本身：
        //   · 顺序：发射器按字段原序输出，故此处也按原序折叠、不排序。排序会把
        //     "顺序不同但集合相同"判为同键，与真实文本不符。字段顺序的来源只有
        //     两处——TOML 数组顺序与 HGL_MATERIAL_HAS_VERTEX_NORMAL 的追加——都是
        //     确定的，无需归一化即可稳定。
        //   · 空名：发射器跳过（不产生宏），这里同样跳过，但仍计入条目数，
        //     使"多一个空条目"也能换键。
        //   · sampler 索引：宏正文是 "<idx>u"，idx 由 sampler.toml 的条目顺序
        //     决定。只折名字会漏掉"重排 sampler.toml"这一整类欠近似，故连 idx
        //     一起折。GetIndex 查不到时返回 ~0u，此时发射器报错且不发宏 ⇒ 该
        //     状态本就产不出可用 SPV，键带上 ~0u 无害。
        // ═══════════════════════════════════════════════════════════════════
        uint64 BuildInjectedMacroHash(const MaterialDefinition &definition)
        {
            hgl::hash::FNV1aHasher64 hasher;

            hasher << static_cast<uint64>(definition.compile_defines.size());
            for (const std::string &name : definition.compile_defines)
            {
                if (name.empty())
                    continue;
                hasher << name;
            }

            hasher << static_cast<uint64>(definition.sampler_names.size());
            for (const std::string &name : definition.sampler_names)
            {
                if (name.empty())
                    continue;
                hasher << name
                       << static_cast<uint64>(
                              SamplerPresetLibrary::Instance().GetIndex(
                                  name.c_str()));
            }

            return hasher;
        }

        /// 构造 fragment 阶段的发射输入（Phase 3.5 与 Phase 4 共用同一份对象）。
        ///
        /// 两处各自拼装 ComposeInput 是危险的：声明键折叠的输入与真正喂给
        /// Compose() 的输入会悄悄分叉，改一处忘一处就静默复用过期 SPV。
        bool BuildFragmentEmissionInput(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            if (!BuildCodeModuleDocument(
                    plan.manifest.IsValid() ? &plan.manifest : nullptr,
                    "fragment",
                    definition.definition_name.c_str(),
                    plan.code_module_document))
            {
                GLogError("[ShaderGen] Generic material code module document build failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }

            FragmentTemplateComposer::ComposeInput &input = plan.compose_input;
            input = FragmentTemplateComposer::ComposeInput{};
            input.resolved_template =
                plan.resolved_render_template.IsValid()
                    ? &plan.resolved_render_template : nullptr;
            input.alpha_test =
                plan.coverage.mode == MaterialCoverageMode::AlphaTest
             || plan.coverage.mode == MaterialCoverageMode::AlphaTestDither;
            input.alpha_cutoff = plan.coverage.alpha_cutoff;
            input.dither =
                plan.coverage.mode == MaterialCoverageMode::Dither
             || plan.coverage.mode == MaterialCoverageMode::AlphaTestDither;
            input.alpha_to_output = plan.alpha_to_output;
            input.fragment_inputs = &plan.stage_interface;
            input.output_contract = &plan.output_contract;
            input.coverage_contract = &plan.coverage;
            input.code_module_document = &plan.code_module_document;
            input.texture_declarations = &definition.texture_declarations;
            return true;
        }

        bool BuildDeclarationKey(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            plan.declaration_fragment = GenericMaterialBuildPlan::DeclarationKey{};

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

            GenericMaterialBuildPlan::DeclarationKey &key =
                plan.declaration_fragment;

            const RenderTemplateID template_id =
                plan.render_template_request->template_id;

            hgl::hash::FNV1aHasher64 base_hasher;
            GLSLIncludeClosureSink base_sink;
            base_sink.hasher = &base_hasher;
            base_sink.include_paths = &key.include_paths;

            const char *template_file =
                GetRenderTemplateFragmentSourceFile(template_id);

            // 以下失败一律非致命：只要算不出来就置 ready=false，让
            // FinalizeProgramLink 回退到文本派生路径，绝不因此让构建失败。
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

            // declaration_roots 只取 request 里的 root，profile 里没有就不进
            // 闭包。Sky 曾把永不发射的 forward_lighting 当 OutputPolicy 根声明
            // （dead module root），使声明闭包多出 forward_lighting 与
            // lighting_interface；已从 MakeSkyProfile / SkySlots 移除。
            SortAnsiStrings(key.include_paths);

            hgl::hash::FNV1aHasher64 module_hasher;
            module_hasher << declaration_manifest.stable_hash
                          << plan.resolved_template_hash
                          << static_cast<uint64>(base_hasher);

            key.template_base_hash = base_hasher;
            key.module_graph_hash = module_hasher;
            key.interface_hash =
                FragmentTemplateComposer::GetInterfaceInputHash(
                    plan.compose_input);

            // 定义正文 = 骨架正文（含其基座 include 内容）+ ComposeInput 的全部
            // 声明字段 + emitter 注入型宏。base_hasher 同时出现在
            // module_graph_hash 里属于有意为之：两个字段语义不同，重叠只会让
            // 失效更保守，不会漏。
            hgl::hash::FNV1aHasher64 definition_hasher;
            definition_hasher
                << FragmentTemplateComposer::GetDeclarationInputHash(
                       plan.compose_input)
                << static_cast<uint64>(base_hasher)
                << BuildInjectedMacroHash(definition);
            key.definition_hash = definition_hasher;

            key.ready = true;
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 3.5b — mesh stage 声明键
        //
        // mesh 侧的 include 全部来自描述符的三个 resolver，它们是
        // (node_cfg, position_format) 的纯函数 ⇒ emit 之前完全可算。
        // 把三条路径拼成一段合成文档再走同一个闭包扫描器，避免为 mesh 另写
        // 一套 include 解析（两套解析迟早会漂移）。
        // ═══════════════════════════════════════════════════════════════════
        bool BuildMeshDeclarationKey(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            GenericMaterialBuildPlan::DeclarationKey &key =
                plan.declaration_mesh;

            const MeshModeDescriptor *desc =
                GetMeshModeDescriptor(plan.mesh_shader_mode);
            const char *template_file =
                GetMeshShaderModeTemplateFile(plan.mesh_shader_mode);

            if (!desc || !template_file)
            {
                GLogWarning(
                    "[ShaderGen] mesh 声明键：模式无描述符，回退文本派生: name=%s",
                    definition.definition_name.c_str());
                return true;
            }

            hgl::hash::FNV1aHasher64 base_hasher;
            GLSLIncludeClosureSink base_sink;
            base_sink.hasher = &base_hasher;
            base_sink.include_paths = &key.include_paths;

            // 骨架正文：含 {{group_size}} / {{max_vertices}} / {{varying_outputs}}
            // 等占位符，正文变化必须让键失效，故整篇折进 base 哈希。
            const std::string &template_text = GetMeshShaderTemplate(template_file);
            if (template_text.empty())
            {
                GLogWarning(
                    "[ShaderGen] mesh 声明键：骨架缺失，回退文本派生: name=%s file=%s",
                    definition.definition_name.c_str(), template_file);
                return true;
            }
            HashShaderLibraryText(
                base_hasher, template_text.data(), template_text.size());
            AccumulateGLSLIncludeClosure(
                base_sink, template_text.data(), template_text.size(), 0);

            // 三个 stage 模块：与 MeshTemplateEmitter 实际发射的 #include 同源
            std::string stage_includes;
            const char *stage_paths[3]{};

            if (desc->resolve_stage1_input)
                stage_paths[0] = desc->resolve_stage1_input(
                    plan.vertex_node_config, plan.position_format);
            if (desc->resolve_stage2_mapping)
                stage_paths[1] = desc->resolve_stage2_mapping(
                    plan.vertex_node_config);
            if (desc->resolve_stage3_projection)
                stage_paths[2] = desc->resolve_stage3_projection(
                    plan.vertex_node_config);

            for (const char *stage_path : stage_paths)
            {
                if (!stage_path || !stage_path[0])
                    continue;
                stage_includes += "#include \"";
                stage_includes += stage_path;
                stage_includes += "\"\n";
            }

            if (!stage_includes.empty())
                AccumulateGLSLIncludeClosure(
                    base_sink, stage_includes.data(), stage_includes.size(), 0);

            // 模式专属资源里由 C++ 硬编码发射的 #include：不经模块依赖，
            // manifest 覆盖不到，与基座 include 同理必须显式纳入，否则改那个
            // 模块不会让 mesh 声明键失效（命中后校验能兜底，但每次都要重编译）。
            if (desc->resolve_declaration_includes)
            {
                int declaration_include_count = 0;
                const char *const *declaration_includes =
                    desc->resolve_declaration_includes(declaration_include_count);

                std::string mode_includes;
                for (int i = 0;
                     declaration_includes && i < declaration_include_count;
                     ++i)
                {
                    if (!declaration_includes[i] || !declaration_includes[i][0])
                        continue;
                    mode_includes += "#include \"";
                    mode_includes += declaration_includes[i];
                    mode_includes += "\"\n";
                }

                if (!mode_includes.empty())
                    AccumulateGLSLIncludeClosure(
                        base_sink, mode_includes.data(), mode_includes.size(), 0);
            }

            // 发射器硬编码的基座 include：不属于任何模块依赖，必须显式纳入。
            if (!AccumulateMeshBaseClosure(base_sink, plan.vertex_node_config))
            {
                GLogWarning(
                    "[ShaderGen] mesh 声明键：基座 include 解析失败，回退文本派生: name=%s",
                    definition.definition_name.c_str());
                return true;
            }

            // 顶点 ABI 文档：VertexABIBuilder 按语义需求/属性格式挑 s1_* 解码
            // 模块，正文由 MeshTemplateEmitter 原样 append。它不是模块依赖，
            // manifest 覆盖不到，必须连正文带闭包一并折进声明键。
            const int abi_block_count = plan.resolved_vertex_input_document.GetBlockCount();
            for (int i = 0; i < abi_block_count; ++i)
            {
                const ShaderDocumentBlock &block =
                    plan.resolved_vertex_input_document.GetBlock(i);
                HashShaderLibraryText(
                    base_hasher, block.text.c_str(),
                    static_cast<size_t>(block.text.Length()));
                AccumulateGLSLIncludeClosure(
                    base_sink, block.text.c_str(),
                    static_cast<size_t>(block.text.Length()), 0);
            }

            SortAnsiStrings(key.include_paths);

            key.template_base_hash = base_hasher;
            key.module_graph_hash = static_cast<uint64>(base_hasher);

            // 顶点输出 ABI + 模式容量：决定 varying 输出声明、max_vertices /
            // max_primitives 与模式专属 #define，全部与生成的文本一一对应。
            // emitter 注入型宏（compile_defines / sampler_names）同样进 mesh 文档，
            // 故与 fragment 侧对称纳入。
            hgl::hash::FNV1aHasher64 definition_hasher;
            definition_hasher << static_cast<uint64>(base_hasher)
                              << plan.varying
                              << VertexNodeConfigResolver::GetHash(
                                     plan.vertex_node_config)
                              << static_cast<uint64>(plan.position_format)
                              << static_cast<uint64>(plan.mesh_shader_mode)
                              << static_cast<uint64>(plan.mesh_max_invocations)
                              << BuildInjectedMacroHash(definition);
            key.definition_hash = definition_hasher;

            hgl::hash::FNV1aHasher64 interface_hasher;
            interface_hasher << plan.varying
                             << VertexNodeConfigResolver::GetHash(
                                    plan.vertex_node_config)
                             << static_cast<uint64>(plan.position_format)
                             << static_cast<uint64>(plan.mesh_shader_mode)
                             << static_cast<uint64>(plan.mesh_max_invocations);
            const int stage_interface_count = plan.stage_interface.GetCount();
            interface_hasher << static_cast<uint64>(stage_interface_count);
            for (int i = 0; i < stage_interface_count; ++i)
                interface_hasher << plan.stage_interface[i];
            key.interface_hash = interface_hasher;

            key.ready = true;
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // definition 预处理（Phase 0）
        //
        // BuildGenericMaterial 与 ComputeMaterialProgramKey 两条路径必须吃**同一份**
        // 预处理结果，否则纯算键与全量键会在 OptionalFaceFallback 变体上分叉。
        // ═══════════════════════════════════════════════════════════════════
        bool PrepareResolvedDefinition(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            MaterialDefinition &out_resolved_definition)
        {
            out_resolved_definition = definition;
            if (definition.vertex_normal_mode
                    == MaterialVertexNormalMode::OptionalFaceFallback
             && request.geometry_vertex_format
             && request.geometry_vertex_format->Find(VertexSemantic::Normal))
            {
                out_resolved_definition.vertex_varying.emit_world_normal = true;
                if (!HasVertexSemanticRequirement(
                        out_resolved_definition, VertexSemantic::Normal))
                {
                    out_resolved_definition.vertex_semantic_requirements.Add(
                        MakeMaterialVertexSemanticRequirement(
                            VertexSemantic::Normal));
                }
                out_resolved_definition.compile_defines.push_back(
                    "HGL_MATERIAL_HAS_VERTEX_NORMAL");
            }

            return !out_resolved_definition.vertex_semantic_requirements.IsEmpty();
        }

        // program link 的三个独立哈希维度（不含 stage 声明键）
        struct ProgramLinkHashes
        {
            uint64 resource_contract_hash = 0;
            uint64 vertex_input_hash = 0;
            uint64 compiler_hash = 0;
        };

        ProgramLinkHashes ComputeProgramLinkHashes(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinitionBuildRequest &request,
            const GenericMaterialBuildPlan &plan)
        {
            ProgramLinkHashes hashes;
            // 资源布局身份 = manifest 内容哈希（2026-10-11 S1：描述符契约层删除）。
            // 等价式保留：原「契约哈希(空契约, manifest_hash)」= FNV1a(manifest_hash, FNV1a(uint32(0)))
            // —— 空契约的条目哈希是常量 ⇒ 逐位同值，stage key / resource_layout_hash 不变、
            // 不触发 SPV 与管线缓存整体失效。
            hashes.resource_contract_hash = ComputeResourceLayoutHash(
                plan.depth_purpose, plan.manifest.stable_hash);
            hashes.vertex_input_hash = request.geometry_vertex_format
                ? request.geometry_vertex_format->GetVertexInputHash() : 0;
            hashes.compiler_hash = contract::GetShaderCompilerProfileHash(profile);
            return hashes;
        }

        // ═══════════════════════════════════════════════════════════════════
        // program link 装配 —— 纯声明路径
        //
        // **只**吃 Phase ≤3.5 的产物：不读 plan.ms / plan.fs，因此可以在发射任何
        // GLSL 文本之前算出与全量路径逐位相同的 program key。
        // 声明键任一未就绪（磁盘闭包读不到）⇒ 返回 false，调用方回退文本派生。
        // ═══════════════════════════════════════════════════════════════════
        bool AssembleProgramLinkFromDeclaration(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            if (!plan.declaration_mesh.ready || !plan.declaration_fragment.ready)
                return false;

            const ProgramLinkHashes hashes =
                ComputeProgramLinkHashes(profile, request, plan);

            // 顶点输入格式是 program_link 的独立维度，但与 mesh stage 的
            // varying 输出声明强耦合，仍然折进 interface_hash（两条路径一致）。
            uint64 mesh_interface_hash = plan.declaration_mesh.interface_hash;
            {
                hgl::hash::FNV1aHasher64 interface_hasher;
                interface_hasher << mesh_interface_hash << hashes.vertex_input_hash;
                mesh_interface_hash = interface_hasher;
            }

            plan.program_link.mesh_stage = BuildDeclarationShaderStageKey(
                ShaderStage::Mesh,
                plan.declaration_mesh.definition_hash,
                plan.declaration_mesh.module_graph_hash,
                mesh_interface_hash,
                hashes.resource_contract_hash,
                hashes.compiler_hash);

            plan.program_link.fragment_stage = BuildDeclarationShaderStageKey(
                ShaderStage::Fragment,
                plan.declaration_fragment.definition_hash,
                plan.declaration_fragment.module_graph_hash,
                plan.declaration_fragment.interface_hash,
                hashes.resource_contract_hash,
                hashes.compiler_hash);

            plan.program_link.resource_layout_hash = hashes.resource_contract_hash;
            plan.program_link.vertex_input_hash = hashes.vertex_input_hash;
            plan.program_link.render_target_hash =
                GetOutputContractHash(plan.output_contract);
            plan.program_link.compiler_hash = hashes.compiler_hash;
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // program link 装配 —— 文本派生回退路径
        //
        // 只在声明键不可用（磁盘闭包读不到）时走：直接用已发射的 GLSL 文本算
        // 摘要。算得贵，但绝不让材质编译失败。mesh / fragment 两侧可独立回退。
        // ═══════════════════════════════════════════════════════════════════
        void AssembleProgramLinkFromText(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            const ProgramLinkHashes hashes =
                ComputeProgramLinkHashes(profile, request, plan);
            const uint64 resource_contract_hash = hashes.resource_contract_hash;
            const uint64 vertex_input_hash = hashes.vertex_input_hash;
            const uint64 compiler_hash = hashes.compiler_hash;

            const GenericMaterialBuildPlan::DeclarationKey *mesh_key =
                plan.declaration_mesh.ready ? &plan.declaration_mesh : nullptr;
            const GenericMaterialBuildPlan::DeclarationKey *fragment_key =
                plan.declaration_fragment.ready
                    ? &plan.declaration_fragment : nullptr;

            uint64 mesh_definition_hash = 0;
            uint64 mesh_module_graph_hash = 0;
            uint64 mesh_interface_hash = 0;

            if (mesh_key)
            {
                mesh_definition_hash = mesh_key->definition_hash;
                mesh_module_graph_hash = mesh_key->module_graph_hash;
                mesh_interface_hash = mesh_key->interface_hash;
            }
            else
            {
                mesh_definition_hash =
                    HashFinalShaderSource(plan.ms.data(), plan.ms.size());

                hgl::hash::FNV1aHasher64 module_graph_hasher;
                module_graph_hasher << ComputeShaderLibraryDependencyHash(
                    plan.ms.data(), plan.ms.size());
                mesh_module_graph_hash = module_graph_hasher;

                hgl::hash::FNV1aHasher64 interface_hasher;
                interface_hasher << mesh_definition_hash;
                mesh_interface_hash = interface_hasher;
            }

            // 顶点输入格式是 program_link 的独立维度，但与 mesh stage 的
            // varying 输出声明强耦合，仍然折进 interface_hash（两条路径一致）。
            {
                hgl::hash::FNV1aHasher64 interface_hasher;
                interface_hasher << mesh_interface_hash << vertex_input_hash;
                mesh_interface_hash = interface_hasher;
            }

            plan.program_link.mesh_stage = BuildDeclarationShaderStageKey(
                ShaderStage::Mesh,
                mesh_definition_hash,
                mesh_module_graph_hash,
                mesh_interface_hash,
                resource_contract_hash,
                compiler_hash);

            uint64 fragment_definition_hash = 0;
            uint64 fragment_module_graph_hash = 0;
            uint64 fragment_interface_hash = 0;

            if (fragment_key)
            {
                fragment_definition_hash = fragment_key->definition_hash;
                fragment_module_graph_hash = fragment_key->module_graph_hash;
                fragment_interface_hash = fragment_key->interface_hash;
            }
            else
            {
                fragment_definition_hash =
                    HashFinalShaderSource(plan.fs.data(), plan.fs.size());

                hgl::hash::FNV1aHasher64 module_graph_hasher;
                module_graph_hasher << plan.manifest.stable_hash
                                    << plan.resolved_template_hash
                                    << ComputeShaderLibraryDependencyHash(
                                           plan.fs.data(), plan.fs.size());
                fragment_module_graph_hash = module_graph_hasher;

                fragment_interface_hash = fragment_definition_hash;
            }

            plan.program_link.fragment_stage = BuildDeclarationShaderStageKey(
                ShaderStage::Fragment,
                fragment_definition_hash,
                fragment_module_graph_hash,
                fragment_interface_hash,
                resource_contract_hash,
                compiler_hash);

            plan.program_link.resource_layout_hash = resource_contract_hash;
            plan.program_link.vertex_input_hash = vertex_input_hash;
            plan.program_link.render_target_hash =
                GetOutputContractHash(plan.output_contract);
            plan.program_link.compiler_hash = compiler_hash;
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

#ifdef _DEBUG
            DumpShaderGenGLSL("mesh", plan.ms);
            DumpShaderGenGLSL("fs", plan.fs);
#endif

            // ── stage key：优先纯声明装配（不读文本），读不到时回退文本派生 ──
            //
            // 声明键是权威路径：它只依赖声明输入，运行时不必跑一遍代码生成就能
            // 得到同一组键 —— ComputeMaterialProgramKey 走的正是这条纯路径。
            // 回退路径保留是因为声明闭包读取依赖 ShaderLibrary 的磁盘状态——
            // 读不到时宁可算贵一点，也不能让材质编译失败。
            if (!AssembleProgramLinkFromDeclaration(profile, request, plan))
                AssembleProgramLinkFromText(profile, request, plan);

            {
                std::vector<AnsiString> text_include_paths;
                CollectShaderLibraryIncludePaths(
                    plan.fs.data(), plan.fs.size(), text_include_paths);
                ReportDeclarationClosureDivergence(
                    definition, plan.declaration_fragment, text_include_paths);
            }

            {
                std::vector<AnsiString> text_include_paths;
                CollectShaderLibraryIncludePaths(
                    plan.ms.data(), plan.ms.size(), text_include_paths);
                ReportDeclarationClosureDivergence(
                    definition, plan.declaration_mesh, text_include_paths);
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
    // ComputeMaterialProgramKey — Phase 3.5 纯算键（不发射 GLSL 文本）
    //
    // 与 BuildGenericMaterial 的相位 1~3.5b 完全同源，只是到此为止：
    // 不跑 GenerateStageSources（Phase 4），不建 ShaderBuildContext。
    // 因此 `out_program_key` 与全量路径 `GetProgramLink().BuildKey()` 必须逐位
    // 相同 —— 这条等价性由回归门 cache.pre-emit-program-key-parity 全变体校验。
    //
    // 探针语义：任何一步失败都只表示"本次算不出键"，不改动全局错误串 ——
    // 调用方随后会走全量路径，由那条路径给出真正的错误诊断。
    // ═══════════════════════════════════════════════════════════════════════
    bool ComputeMaterialProgramKey(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition,
        ShaderProgramKey &out_program_key)
    {
        out_program_key = ShaderProgramKey{};

        MaterialDefinition resolved_definition;
        if (!PrepareResolvedDefinition(definition, request, resolved_definition))
            return false;

        const AnsiString saved_error = s_last_build_generic_material_error;

        GenericMaterialBuildPlan plan{};
        plan.primitive_type = request.primitive_type;

        if (ResolvePurposeAndCoverage(resolved_definition, request, plan)
         && ResolveVertexABI(resolved_definition, request, plan))
        {
            ResolveMeshStageMode(profile, resolved_definition, plan);

            if (BuildResourceContract(resolved_definition, plan)
             && BuildFragmentEmissionInput(resolved_definition, plan)
             && BuildDeclarationKey(resolved_definition, plan)
             && BuildMeshDeclarationKey(resolved_definition, plan)
             && AssembleProgramLinkFromDeclaration(profile, request, plan))
            {
                out_program_key = plan.program_link.BuildKey();
                return true;
            }
        }

        s_last_build_generic_material_error = saved_error;
        return false;
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
        MaterialDefinition resolved_definition;
        if (!PrepareResolvedDefinition(definition, request, resolved_definition))
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

        ResolveMeshStageMode(profile, resolved_definition, plan);

        if (!BuildResourceContract(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildResourceContract failed";
            GLogError("[BuildGenericMaterial] BuildResourceContract failed");
            return nullptr;
        }

        // fragment 发射输入必须先于 BuildDeclarationKey：声明键折叠的就是这份
        // ComposeInput，Phase 4 也原样复用它——两处共用一份对象，避免"键覆盖
        // 的输入"与"实际发射的输入"悄悄分叉。
        if (!BuildFragmentEmissionInput(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildFragmentEmissionInput failed";
            GLogError("[BuildGenericMaterial] BuildFragmentEmissionInput failed");
            return nullptr;
        }

        if (!BuildDeclarationKey(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildDeclarationKey failed";
            GLogError("[BuildGenericMaterial] BuildDeclarationKey failed");
            return nullptr;
        }

        if (!BuildMeshDeclarationKey(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildMeshDeclarationKey failed";
            GLogError("[BuildGenericMaterial] BuildMeshDeclarationKey failed");
            return nullptr;
        }

        if (!GenerateStageSources(
                resolved_definition, document_capture, plan))
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
