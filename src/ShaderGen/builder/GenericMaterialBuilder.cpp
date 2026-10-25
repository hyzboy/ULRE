/// GenericMaterialBuilder.cpp — phase-split generic material compilation.
///
/// This file is the behavior-preserving decomposition of the former
/// BuildGenericMaterial function in MaterialDefinitionRegistry.cpp. Every
/// statement executes in the original order; intermediate values travel via
/// GenericMaterialBuildPlan. Do not reorder statements here — stage keys and
/// resource contracts depend on the exact hash input sequence.

#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialCoverageContract.h>
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

        /// 去掉某条顶点语义需求（面法线降级用）。
        bool RemoveVertexSemanticRequirement(
            MaterialDefinition &definition,
            const VertexSemantic semantic) noexcept
        {
            for (int i = definition.vertex_semantic_requirements.GetCount() - 1;
                 i >= 0;
                 --i)
            {
                if (GetVertexSemanticFromShaderCodeModuleSemantic(
                        definition.vertex_semantic_requirements[i].semantic)
                    != semantic)
                    continue;

                definition.vertex_semantic_requirements.Delete(i);
                return true;
            }
            return false;
        }

        /// 委托 mtl 层单一口径（`MaterialRecipe.h::HasMaterialVertexSemanticRequirement`）：
        /// 同一个判据还被 ECS 渲染层（几何错配判定）与契约测试使用，不能各写一份。
        bool HasVertexSemanticRequirement(
            const MaterialDefinition &definition,
            const VertexSemantic semantic) noexcept
        {
            return HasMaterialVertexSemanticRequirement(definition, semantic);
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
                    if (IsMaterialVertexSemanticRequiredForVarying(
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
            // 求解结果必须回写到 ABI 求解用的那份定义：`vertex_node_config` 有四个
            // 来源（request 覆盖 > recipe > definition > 默认），而 `definition` 里
            // 存的是**最弱的那一层**。不回写就会出现「mesh 发射器用 recipe 的
            // PixelToLocal，ABI 对账却用 definition 的 Passthrough3D」——
            // 位置分量数一个 2 一个 3，`CheckPositionComponentCount` 必然硬失败
            // （DrawTriangle 的 VertexColor 就是这么炸的）。
            plan.vertex_definition.vertex_node_config = plan.vertex_node_config;
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
        // 生成的 GLSL 无关；但它们决定 max_vertices/max_primitives 与部分
        // #define，所以必须早于 Phase 4 定下来。
        // ═══════════════════════════════════════════════════════════════════
        bool ResolveMeshStageMode(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
            // 模式选择优先级：definition.mesh_shader_mode > primitive_type 推断
            if (IsCharQuadMode(definition.mesh_shader_mode))
            {
                plan.mesh_shader_mode = MeshShaderMode::CharQuad;

                // ── CharQuad 的线程数是**锁死**契约，不能被钳到更小 ──
                //
                // CPU 端 dispatch（TextRenderPipeline）按 TEXT_CHARQUAD_MAX_INVOCATIONS
                // 整除算组数，它拿不到 shader 的 local_size_x。于是生成侧一旦出现
                // 更小的 group size，就变成「shader 每组处理不足 64 个字符，而 CPU
                // 仍按 64 派发」—— 尾部字符整段不显示，且没有任何报错。
                //
                // 两条会缩小的路径都在这里拦下：
                //   ① 材质 TOML 的 mesh_shader_max_invocations 不是 64；
                //   ② 设备能力上限（ClampMeshInvocationsByDevice）< 64。
                // ② 在符合 Vulkan 规范的设备上不会发生（保证 maxMeshOutputVertices
                // ≥ 256、max_mesh_work_group_size_x ≥ 128 ⇒ 64 恒可达），一旦发生
                // 说明设备不合规，宁可构建失败也不能发出与 CPU 派发不符的 shader。
                const uint32_t declared = definition.mesh_shader_max_invocations;
                if (declared != 0 && declared != TEXT_CHARQUAD_MAX_INVOCATIONS)
                {
                    GLogError(
                        "[ShaderGen] CharQuad 线程数契约被破坏：definition=%s "
                        "mesh_shader_max_invocations=%u 必须为 %u"
                        "（CPU dispatch 固定按该常量算组数，改小会整段漏字）",
                        definition.definition_id.c_str(),
                        static_cast<unsigned>(declared),
                        static_cast<unsigned>(TEXT_CHARQUAD_MAX_INVOCATIONS));
                    return false;
                }

                const uint32_t clamped = ClampMeshInvocationsByDevice(
                    profile, plan.mesh_shader_mode,
                    TEXT_CHARQUAD_MAX_INVOCATIONS);
                if (clamped != TEXT_CHARQUAD_MAX_INVOCATIONS)
                {
                    GLogError(
                        "[ShaderGen] CharQuad 线程数契约无法兑现：definition=%s "
                        "设备能力上限折算后 group_size=%u < %u"
                        "（CPU dispatch 固定按 %u 算组数，退让会整段漏字）",
                        definition.definition_id.c_str(),
                        static_cast<unsigned>(clamped),
                        static_cast<unsigned>(TEXT_CHARQUAD_MAX_INVOCATIONS),
                        static_cast<unsigned>(TEXT_CHARQUAD_MAX_INVOCATIONS));
                    return false;
                }

                // CharQuad: 每线程 4 顶点，64 × 4 = 256 = Vulkan 保证的
                // maxMeshOutputVertices 下限（TEXT_CHARQUAD_MAX_INVOCATIONS 与
                // CPU dispatch 共享）。
                plan.mesh_max_invocations = TEXT_CHARQUAD_MAX_INVOCATIONS;
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

            return true;
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
            const MaterialDefinitionBuildRequest &request,
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

            // ── 片段阶段：由领域主着色器物理固定时不发射 ────────────────────
            //
            // 固定源直接成为最终片段文本。它同时也喂 stage key 的
            // definition_hash——键与真正被编译的文本是同一份字节。
            if (request.fixed_fragment_source)
            {
                plan.fs = request.fixed_fragment_source;
                return true;
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

        /// 递归把文档里 `#include "..."` 指向的模块**正文**并入哈希。
        ///
        /// 最终文档对 ShaderLibrary 里的 GLSL 模块只保留 include 指令，真正的
        /// 展开发生在编译期。因此只哈希文档正文不够：改一个被 include 的 .glsl
        /// 不会改变 stage key，磁盘上旧的 SPV 会被永久命中复用（曾导致长时间
        /// 静默运行陈旧阴影代码）。这里按 GLSLCompiler 同一 include 根递归遍历，
        /// 让「闭包内任何一个文件的任何一个字节变化」都必然换键。
        void AccumulateGLSLIncludeClosure(
            hgl::hash::FNV1aHasher64 &hasher,
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

                        HashShaderLibraryText(
                            hasher,
                            include_path.c_str(),
                            static_cast<size_t>(include_path.Length()));

                        const OSString module_path =
                            ResolveShaderLibraryModulePath(include_path);

                        if (module_path.IsEmpty())
                        {
                            HashShaderLibraryText(
                                hasher, "<module-missing>", 16);
                        }
                        else
                        {
                            const auto module_bytes =
                                hgl::LoadFileToDataArray<uint8>(module_path);

                            HashShaderLibraryText(
                                hasher,
                                reinterpret_cast<const char *>(module_bytes.data()),
                                module_bytes.size());
                            AccumulateGLSLIncludeClosure(
                                hasher,
                                reinterpret_cast<const char *>(module_bytes.data()),
                                module_bytes.size(), depth + 1);
                        }
                    }
                }

                line_begin = line_end + 1;
            }
        }

        /// 一份最终文档的 ShaderLibrary 依赖闭包哈希（同一文档只算一次）
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
            AccumulateGLSLIncludeClosure(hasher, document, length, 0);
            const uint64 dependency_hash = hasher;

            {
                hgl::ThreadMutexLock lock(&dependency_hash_mutex);
                dependency_hash_cache.Add(document_hash, dependency_hash);
            }

            return dependency_hash;
        }

        /// 构造 fragment 阶段的发射输入。
        ///
        /// ComposeInput 是「声明侧」与「发射侧」之间唯一的一份对象：interface_hash
        /// 直接吃它，Phase 4 原样把它喂给 Compose()。两处各自拼装会悄悄分叉。
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

        /// 校验并归一化输出附件契约。
        ///
        /// 必须先于 Phase 4：Compose() 读 plan.output_contract，契约非法时要
        /// 在这里给出定位明确的诊断，而不是在几千行 GLSL 里失败。
        bool BuildFragmentOutputContract(
            const MaterialDefinition &definition,
            GenericMaterialBuildPlan &plan)
        {
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

            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // definition 预处理（Phase 0）
        //
        // 法线来源的归一化只在这里做一次：预处理结果参与 vertex ABI / resource
        // manifest / 最终发射，多处各自归一化必然分叉。
        // ═══════════════════════════════════════════════════════════════════
        // 法线需求的判据在 mtl 层单一口径（`MaterialRecipe.h::MaterialWantsNormalLighting` /
        // `MaterialRequiresVertexNormal` / `MaterialToleratesFaceNormalFallback`）—— 同一个
        // 判据还要被 ECS 渲染层用来判定几何顶点格式错配并回退错误棋盘格，两处口径必须同源。

        /// 归一化「法线从哪来」，两条路：
        ///   · 顶点格式有 Normal ⇒ 走顶点法线（补齐 Normal 需求 + `HGL_MATERIAL_HAS_VERTEX_NORMAL`）；
        ///   · 顶点格式没有 Normal ⇒ 面法线降级（剥 N/T/B 需求 + `HGL_MATERIAL_FACE_NORMAL_FALLBACK`）。
        bool PrepareResolvedDefinition(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            MaterialDefinition &out_resolved_definition)
        {
            out_resolved_definition = definition;

            if (!request.geometry_vertex_format
             || !MaterialWantsNormalLighting(definition))
                return !out_resolved_definition.vertex_semantic_requirements.IsEmpty();

            if (request.geometry_vertex_format->Find(VertexSemantic::Normal))
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

                return !out_resolved_definition.vertex_semantic_requirements.IsEmpty();
            }

            // ── 面法线降级（顶点缺法线，但材质要法线光照）────────────────────
            //
            // 顶点格式没有 Normal 语义时，绝不能保留 Normal/Tangent/Bitangent 需求：
            // `s1_ntb*.glsl` 会按 ABI 从顶点 SSBO 的 Normal 槽取数，而那条通道根本没上传
            // ⇒ 读到的是别的顶点数据/垃圾值（画面表现为随机明暗，比没光更坏）。
            // 因此三者一并剥掉，法线改由**片段着色器用屏幕空间导数求三角形面法线**
            //（`HGL_MATERIAL_FACE_NORMAL_FALLBACK`，装配处见
            //  contract::BuildGLSLMaterialSurfaceInput）。
            //
            // 语义定稿（2026-10-24 用户两次拍板，后者覆盖前者）：
            //   · 缺顶点法线**必须看得见** —— 面法线平面着色看起来「像正常的低模」，
            //     问题会被这层伪装掩掉 ⇒ ECS 渲染层把「材质要顶点法线、几何没有」判为
            //     `MissingVertexNormal` 几何错配，**整条配方换成错误棋盘格（黄）**，本函数
            //     根本不会被调用到（见 `RenderPrimitiveCollectSystem::BuildResolvedRecipe`）；
            //   · 这里的降级因此只剩两类合法/兜底场景：
            //       ① `normal_mode = OptionalFaceFallback` 的材质（棋盘格自身）——
            //          面法线就是它声明的行为，静默降级；
            //       ② ECS 层没拿到几何顶点格式（如离线/测试路径）—— 兜底，不剔除图元。
            RemoveVertexSemanticRequirement(
                out_resolved_definition, VertexSemantic::Normal);
            RemoveVertexSemanticRequirement(
                out_resolved_definition, VertexSemantic::Tangent);
            RemoveVertexSemanticRequirement(
                out_resolved_definition, VertexSemantic::Bitangent);

            // 面法线要用世界位置对屏幕导数求叉积 ⇒ 必须输出世界位置。
            out_resolved_definition.vertex_varying.emit_world_pos = true;
            out_resolved_definition.vertex_varying.emit_world_normal = false;
            out_resolved_definition.compile_defines.push_back(
                "HGL_MATERIAL_FACE_NORMAL_FALLBACK");

            // 容忍者（棋盘格）静默；必须顶点法线却走到这里 = ECS 层漏判 ⇒ 出声。
            if (MaterialRequiresVertexNormal(definition))
            {
                GLogError(
                    u8"[ShaderGen] 材质 %s 必须要顶点法线（normal_mode=%s，ntb_module=%s），但几何"
                    u8"顶点格式缺少 Normal 语义 => 兜底降级为三角形面法线光照（cross(dFdx,dFdy)）。"
                    u8"这是**几何顶点格式错配**，正常应已由 ECS 渲染层回退为错误棋盘格",
                    definition.definition_id.c_str(),
                    GetMaterialVertexNormalModeName(definition.vertex_normal_mode),
                    definition.ntb_module_name ? definition.ntb_module_name : "");
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
        // 文本之外的注入内容
        //
        // `compile_defines` 与 `sampler_names` 不是 Phase 4 文档的一部分：它们在
        // 收尾阶段（`BuildMaterialStageDocument`）才作为 Define 块插进最终 GLSL，
        // 因此只哈希文档文本**看不见**它们。两份仅在这两个列表上不同的材质会算出
        // 同一个 stage key，然后复用对方编译出的 SPV —— 静默、且看起来完全正常。
        //
        // sampler 名必须连带解析后的索引一起折：宏体是 `#define XSampler 7u`，
        // 换一行 sampler.toml 就整体错位。
        // ═══════════════════════════════════════════════════════════════════
        uint64 ComputeInjectedSourceHash(const MaterialDefinition &definition)
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

        // ═══════════════════════════════════════════════════════════════════
        // program link 装配 —— 唯一的键派生路径
        //
        // 键**只**从 Phase 4 发射出的文档文本派生：definition_hash 吃整份文本的
        // 摘要，module_graph_hash 吃文本的 `#include` 闭包指纹，因此「发射出的
        // 任何字节发生变化」都必然改变键 —— 不需要再维护一份手写的「哪些文件
        // 算数」清单去跟发射器同步。
        //
        // 这里刻意不做「声明输入 ≈ 发射结果」的对齐假设：早期版本额外算一份
        // 预发射声明键并逐位比对两者，实际结果是同一份信息维护两遍、且两遍都
        // 会漂。文本是唯一事实。
        //
        // 代价是键必须等 Phase 4 发射完才能算；用不起早退的地方由上层用变体键
        //（`mtl::ComputeMaterialProgramManifestKey`，纯内存）做一级缓存补偿。
        // ═══════════════════════════════════════════════════════════════════
        void AssembleProgramLinkFromSource(
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            const ProgramLinkHashes hashes =
                ComputeProgramLinkHashes(profile, request, plan);
            const uint64 resource_contract_hash = hashes.resource_contract_hash;
            const uint64 vertex_input_hash = hashes.vertex_input_hash;
            const uint64 compiler_hash = hashes.compiler_hash;
            const uint64 injected_source_hash =
                ComputeInjectedSourceHash(definition);

            const uint64 mesh_definition_hash =
                HashFinalShaderSource(plan.ms.data(), plan.ms.size());

            hgl::hash::FNV1aHasher64 mesh_module_graph_hasher;
            mesh_module_graph_hasher << injected_source_hash
                                     << ComputeShaderLibraryDependencyHash(
                                            plan.ms.data(), plan.ms.size());
            const uint64 mesh_module_graph_hash = mesh_module_graph_hasher;

            // 顶点输入格式是 program_link 的独立维度，但与 mesh stage 的
            // varying 输出声明强耦合，仍然折进 interface_hash。
            hgl::hash::FNV1aHasher64 mesh_interface_hasher;
            mesh_interface_hasher << mesh_definition_hash << vertex_input_hash;
            const uint64 mesh_interface_hash = mesh_interface_hasher;

            plan.program_link.mesh_stage = BuildShaderStageKey(
                ShaderStage::Mesh,
                mesh_definition_hash,
                mesh_module_graph_hash,
                mesh_interface_hash,
                resource_contract_hash,
                compiler_hash);

            const uint64 fragment_definition_hash =
                HashFinalShaderSource(plan.fs.data(), plan.fs.size());

            // 片元的闭包指纹额外吃掉 manifest、模板哈希与注入项：模块集合相同的
            // 两份文档，只要资源绑定、模板骨架或注入宏不同，也必须是不同的 stage。
            hgl::hash::FNV1aHasher64 fragment_module_graph_hasher;
            fragment_module_graph_hasher << plan.manifest.stable_hash
                                         << plan.resolved_template_hash
                                         << injected_source_hash
                                         << ComputeShaderLibraryDependencyHash(
                                                plan.fs.data(), plan.fs.size());

            // 接口维度：片元输入 varying + 输出附件 + alpha 写输出。它不吃源
            // 文本，只吃契约形状——把「接口变了」与「实现变了」在键里分开，
            // 排查失效原因时能一眼看出是哪一类。
            //
            // 领域固定主着色器路径没有 ComposeInput（片段不发射），接口形状由
            // 调用方按「领域 + purpose + 覆盖判定」给出。
            const uint64 fragment_interface_hash =
                request.fixed_fragment_interface_hash != 0
                    ? request.fixed_fragment_interface_hash
                    : FragmentTemplateComposer::GetInterfaceInputHash(
                            plan.compose_input);

            plan.program_link.fragment_stage = BuildShaderStageKey(
                ShaderStage::Fragment,
                fragment_definition_hash,
                fragment_module_graph_hasher,
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

            // ── stage key：从最终发射文本派生（唯一路径）──
            AssembleProgramLinkFromSource(profile, definition, request, plan);

            config.program_link = &plan.program_link;
            config.material_private_data = definition.material_private_data;
            config.defer_finalize = request.defer_finalize;
            config.fixed_fragment_source = request.fixed_fragment_source;
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

        if (!ResolveMeshStageMode(profile, resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "ResolveMeshStageMode failed";
            GLogError("[BuildGenericMaterial] ResolveMeshStageMode failed");
            return nullptr;
        }

        if (!BuildResourceContract(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildResourceContract failed";
            GLogError("[BuildGenericMaterial] BuildResourceContract failed");
            return nullptr;
        }

        // fragment 发射输入先建好：interface_hash 直接吃这份 ComposeInput，
        // Phase 4 也原样复用它——两处共用一份对象，避免"声明侧看到的"与
        // "实际发射的"悄悄分叉。
        // 片段阶段已由领域主着色器物理固定时没有可发射的东西，整条链跳过。
        if (!request.fixed_fragment_source
         && !BuildFragmentEmissionInput(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildFragmentEmissionInput failed";
            GLogError("[BuildGenericMaterial] BuildFragmentEmissionInput failed");
            return nullptr;
        }

        if (!BuildFragmentOutputContract(resolved_definition, plan))
        {
            if (s_last_build_generic_material_error.IsEmpty())
                s_last_build_generic_material_error = "BuildFragmentOutputContract failed";
            GLogError("[BuildGenericMaterial] BuildFragmentOutputContract failed");
            return nullptr;
        }

        if (!GenerateStageSources(
                resolved_definition, request, document_capture, plan))
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
