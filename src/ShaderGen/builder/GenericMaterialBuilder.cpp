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
#include <hgl/mtl/MeshTemplateComposer.h>
#include <hgl/graph/font/CharQuadConfig.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/mtl/VertexNodeConfigResolver.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/ShaderCodeModuleCapabilityResolver.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include "compile/MaterialShaderEmitter.h"
#include "builder/DefinitionDescriptorBuilder.h"
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
                   "template=%s validation=%s",
                   definition.definition_name.c_str(),
                   GetRenderTemplateName(
                       plan.render_template_request->template_id),
                   GetRenderTemplateValidationErrorName(diagnostic.error));
                return false;
            }
            plan.resolved_template_hash =
                plan.render_template_request->GetHash();
            if (!ResolveRenderTemplate(
                   *plan.render_template_request,
                   GetShaderCodeModuleRegistry(),
                   plan.resolved_render_template,
                   diagnostic))
            {
                GLogError(
                   "[ShaderGen] Render template resolution failed: name=%s "
                   "template=%s validation=%s",
                   definition.definition_name.c_str(),
                   GetRenderTemplateName(
                       plan.render_template_request->template_id),
                   GetRenderTemplateValidationErrorName(diagnostic.error));
                return false;
            }
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
            plan.resolved_provider_document.Clear();
            plan.resolved_provider_graph_hash = 0;

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
                if (!resolved_abi.provider_glsl.empty())
                {
                    ShaderDocumentSource provider_source;
                    provider_source.stage = "mesh";
                    provider_source.module = "vertex-provider";
                    provider_source.logical_name = "MeshTemplateEmitter.Provider";
                    plan.resolved_provider_document.Add(
                        ShaderDocumentBlockKind::Module,
                        AnsiString(resolved_abi.provider_glsl.c_str()),
                        provider_source);
                }
                plan.resolved_provider_graph_hash = resolved_abi.provider_graph_hash;
                {
                    hgl::hash::FNV1aHasher64 h;
                    h << VertexNodeConfigResolver::GetHash(plan.vertex_node_config)
                      << resolved_abi.provider_graph_hash;
                    plan.resolved_provider_graph_hash = h;
                }
            }
            return true;
        }

        // ═══════════════════════════════════════════════════════════════════
        // Phase 3 — resource manifest / descriptors / descriptor contract
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
            if (!BuildShaderCodeResourceManifest(
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
            // 顶点需求真源统一：描述符与模块 include 必须来自同一份「变体有效 definition」。
            // plan.vertex_definition 已在 Phase 2 按 effective_vertex_varying 裁剪过
            // vertex_semantic_requirements（depth 变体去掉 UV/NTB 等）——原先此处用原始
            // definition，导致**模块侧已裁剪、描述符侧未裁剪**：depth mesh shader 不声明
            // VertexUV/VertexNTB buffer，但 set layout 仍含这两个 binding，且运行期绑定
            // 会去找几何的 UV/NTB VAB（几何未提供即报 no resource）。
            plan.descriptors =
                BuildDescriptorsFromDefinition(plan.vertex_definition);
            if (plan.depth_purpose)
            {
                plan.descriptors.erase(
                    std::remove_if(
                        plan.descriptors.begin(),
                        plan.descriptors.end(),
                        [&](const SerializedDescriptorEntry &entry)
                        {
                            if (entry.semantic == DescriptorSemantic::SkyInfo)
                                return true;
                            // Material 集已退场；仅按语义裁剪变体差异项
                            if (entry.semantic == DescriptorSemantic::MaterialColorPalette)
                                return !plan.effective_vertex_varying.
                                    emit_vertex_color_from_palette;
                            // 其余（L2W/MeshDrawParams/数据槽/UBO）深度变体恒保留
                            return false;
                        }),
                    plan.descriptors.end());
            }
            // A6-2b-b1：mesh_draw_params 不再经契约声明——参数行本体走 BDA
            //（buffer_reference，模板恒发 rows[gl_DrawID] 加载点），无描述符无 set；
            // 契约条目（原仅 schema 记录 + PerObject layout binding 空洞）删除。
            if (!plan.manifest.IsValid())
            {
                GLogError("[ShaderGen] Generic material resource contract failed: name=%s error=%s",
                          definition.definition_name.c_str(),
                          GetShaderCodeResourceManifestErrorName(plan.manifest.error));
                return false;
            }
            if (!BuildDescriptorContract(
                    plan.descriptors, plan.descriptor_contract))
            {
                GLogError(
                    "[ShaderGen] Material descriptor contract build failed: name=%s",
                    definition.definition_name.c_str());
                return false;
            }
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
            MeshTemplateComposer mesh_composer;
            MeshTemplateComposer::ComposeInput mesh_compose_input{};
            mesh_compose_input.node_config = plan.vertex_node_config;
            mesh_compose_input.varying_config = plan.varying;
            mesh_compose_input.position_format = plan.position_format;
            mesh_compose_input.mode = ms_mode;
            mesh_compose_input.max_invocations = max_invocations;
            mesh_compose_input.resolved_input_document =
                &plan.resolved_vertex_input_document;
            mesh_compose_input.provider_document =
                &plan.resolved_provider_document;
            mesh_compose_input.stage_interface = &plan.stage_interface;
            if (!mesh_composer.Compose(mesh_compose_input, mesh_document))
            {
                s_last_build_generic_material_error = "mesh_composer.Compose failed";
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
            MaterialOutputContractDiagnostic output_diagnostic{};
            if (!BuildMaterialOutputContract(
                    plan.purpose,
                    plan.output_contract,
                    output_diagnostic))
            {
                s_last_build_generic_material_error = "BuildMaterialOutputContract failed";
                GLogError(
                    "[ShaderGen] Material output contract build failed: name=%s error=%s",
                    definition.definition_name.c_str(),
                    GetMaterialOutputContractErrorName(
                        output_diagnostic.error));
                return false;
            }
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
        // -I <ShaderLibrary> 与 -I <ShaderLibrary>/common 搜索并拼接源文件）。
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

        /// 按 GLSLCompiler 的 include 搜索顺序解析模块文件的物理路径
        OSString ResolveShaderLibraryModulePath(const AnsiString &include_path)
        {
            if (include_path.IsEmpty())
                return OSString();

            const std::string library_root = GetShaderLibraryPath();
            if (library_root.empty())
                return OSString();

            std::string relative(include_path.c_str());
            for (char &ch : relative)
                if (ch == '\\')
                    ch = '/';

            const OSString root_path = ToOSString(library_root.c_str());
            const OSString direct_path =
                root_path + OS_TEXT("/") + ToOSString(relative.c_str());
            if (filesystem::FileExist(direct_path))
                return direct_path;

            const OSString common_path =
                root_path + OS_TEXT("/common/") + ToOSString(relative.c_str());
            if (filesystem::FileExist(common_path))
                return common_path;

            return OSString();
        }

        /// 递归收集文档里的 `#include "..."`，把闭包内每个模块的正文并入哈希
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

                    if (cursor < line_end && text[cursor] == '"')
                    {
                        const size_t name_begin = ++cursor;
                        while (cursor < line_end && text[cursor] != '"')
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
                            HashShaderLibraryText(hasher, "<module-missing>", 16);
                        }
                        else
                        {
                            const auto module_bytes =
                                hgl::LoadFileToDataArray<uint8>(module_path);
                            const char *module_text =
                                reinterpret_cast<const char *>(module_bytes.data());

                            HashShaderLibraryText(hasher, module_text, module_bytes.size());
                            AccumulateGLSLIncludeClosure(
                                hasher, module_text, module_bytes.size(), depth + 1);
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

            const uint64 document_hash = HashFinalShaderSource(document, length);

            {
                hgl::ThreadMutexLock lock(&dependency_hash_mutex);

                uint64 cached_hash = 0;
                if (dependency_hash_cache.Get(document_hash, cached_hash))
                    return cached_hash;
            }

            hgl::hash::FNV1aHasher64 hasher;
            AccumulateGLSLIncludeClosure(hasher, document, length, 0);
            const uint64 dependency_hash = hasher;

            {
                hgl::ThreadMutexLock lock(&dependency_hash_mutex);
                dependency_hash_cache.Add(document_hash, dependency_hash);
            }

            return dependency_hash;
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
                request.primitive_type,
                plan.descriptors.data(), static_cast<uint32>(plan.descriptors.size())
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
            config.descriptor_contract = &plan.descriptor_contract;
            const uint64 resource_contract_hash =
                GetDescriptorContractHash(
                    plan.descriptor_contract,
                    plan.depth_purpose ? 0 : plan.manifest.stable_hash);
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
            mesh_module_graph_hasher << plan.resolved_provider_graph_hash
                                     << mesh_shader_library_hash;
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

        ShaderBuildContext *result = CompileMaterial(
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
            s_last_build_generic_material_error = "CompileMaterial failed";
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        }
        return result;
    }
}
