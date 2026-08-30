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
#include <hgl/mtl/CompositorAssembler.h>
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
#include <hgl/mtl/GLSLCodeModuleCapabilityResolver.h>
#include "3d/DefinitionDescriptorBuilder.h"
#include "common/VertexVaryingConfig.h"
#include "common/MeshShaderAssembler.h"
#include "common/GenericMaterialBuilder.h"

#include <cstring>
#include <vector>
#include <algorithm>

namespace hgl::graph::mtl
{

    namespace
    {
        // 设备能力推导：group size ≤ min(max_mesh_work_group_size_x,
        //   floor(max_mesh_output_vertices / 每线程顶点数),
        //   floor(max_mesh_output_primitives / 每线程图元数))。
        // 拒绝生成侧硬编码——设备上限由主程序从物理设备实测后经 profile 传入；
        // profile 为 null 或 limits 未填（0）时退回理想值。
        // VertexPassthrough 向下取整到 3 的倍数（组内三角形不跨组，MeshShaderAssembler % 3 守卫）。
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
            if (mode == MeshShaderMode::VertexPassthrough && result > 0)
                result -= result % 3u;   // 3 的倍数（T2.4 守卫要求）

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

        const GLSLCodeModuleDefinition *FindSelectedProviderModule(
            const GLSLCodeModuleRegistry &registry,
            const char *path)
        {
            if (!path || !path[0])
                return nullptr;

            const GLSLCodeModuleDefinition *definition = registry.FindByName(path);
            if (definition)
                return definition;

            const char *stem = path;
            const char *dot = nullptr;
            for (const char *cursor = path; *cursor; ++cursor)
            {
                if (*cursor == '/' || *cursor == '\\')
                {
                    stem = cursor + 1;
                    dot = nullptr;
                }
                else if (*cursor == '.' && !dot)
                    dot = cursor;
            }

            const AnsiString name = dot ? AnsiString(stem, int(dot - stem)) : AnsiString(stem);
            return registry.FindByName(name.c_str());
        }

        // T3：surface → 光照管线配置（单一真源）
        // Lit 的 forward_lighting/lighting_algorithm 传 nullptr——走 CompositorAssembler
        // 的 kModuleSlots 默认路径（forward_pbr 等），模块路径只存在于 Assembler 一处，
        // 不再重复 override（消除双真源）。
        struct SurfaceLightingConfig
        {
            bool        enable_scene_lighting;
            const char *sky_module;               // nullptr = 不注入 sky
            const char *forward_lighting_module;  // nullptr = 走 Assembler 默认
            const char *lighting_algorithm_module;// nullptr = 走 Assembler 默认
        };

        const SurfaceLightingConfig *GetSurfaceLightingConfig(
            const SurfaceType surface) noexcept
        {
            switch (surface)
            {
            case SurfaceType::Unlit:
            case SurfaceType::Sky:
            {
                // 无场景光照：flat 管线（无 sky 大气、无 PBR）
                static const SurfaceLightingConfig cfg =
                    { false, nullptr,
                      "compositor/flat_lighting.glsl",
                      "lighting/forward_flat.glsl" };
                return &cfg;
            }
            case SurfaceType::Lit:
            {
                // 场景光照：PBR + 大气（模块走 Assembler 默认路径）
                static const SurfaceLightingConfig cfg =
                    { true, "sky/sky_atmosphere.glsl", nullptr, nullptr };
                return &cfg;
            }
            default:
                return nullptr;
            }
        }

        // Phase 1 — purpose / coverage / varying / stage interface
        // (originally MaterialDefinitionRegistry.cpp:235-305)
        // ═══════════════════════════════════════════════════════════════════
        bool ResolvePurposeAndCoverage(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            GenericMaterialBuildPlan &plan)
        {
            plan.purpose =
                request.override_shader_program_purpose
                    ? request.shader_program_purpose
                    : GetShaderProgramPurpose(
                        definition.compositor_pass);
            if (!BuildMaterialCoverageContract(
                    definition,
                    request.recipe,
                    plan.purpose,
                    plan.coverage))
                return false;
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
                        GetVertexSemanticFromGLSLCodeModuleSemantic(
                            requirement.semantic);
                    if (IsVertexSemanticRequiredForVarying(
                            semantic, plan.effective_vertex_varying))
                    {
                        plan.vertex_definition.vertex_semantic_requirements.Add(
                            requirement);
                    }
                }
            }

            plan.varying = VertexVaryingConfig{};
            plan.varying.emit_data_index_id = plan.effective_vertex_varying.emit_data_index_id;
            plan.varying.emit_vertex_color = plan.effective_vertex_varying.emit_vertex_color;
            plan.varying.emit_uv0 = plan.effective_vertex_varying.emit_uv0;
            plan.varying.emit_world_pos = plan.effective_vertex_varying.emit_world_pos;
            plan.varying.emit_world_normal = plan.effective_vertex_varying.emit_world_normal;
            plan.varying.emit_luminance = plan.effective_vertex_varying.emit_luminance;
            plan.varying.emit_frag_direction = plan.effective_vertex_varying.emit_frag_direction;
            plan.varying.use_transform_id_attr = plan.effective_vertex_varying.use_transform_id_attr;
            plan.varying.emit_vertex_color_from_palette = plan.effective_vertex_varying.emit_vertex_color_from_palette;
            plan.varying.emit_style_id = plan.effective_vertex_varying.emit_style_id;

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
            plan.resolved_vertex_input_glsl.clear();
            plan.resolved_provider_glsl.clear();
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
                plan.resolved_vertex_input_glsl = resolved_abi.vertex_input_glsl.c_str();
                plan.resolved_provider_glsl = resolved_abi.provider_glsl;
                plan.resolved_provider_graph_hash = resolved_abi.provider_graph_hash;
                {
                    hgl::hash::FNV1aHasher64 h;
                    h << VertexNodeConfigResolver::GetHash(plan.vertex_node_config)
                      << resolved_abi.provider_graph_hash;
                    plan.resolved_provider_graph_hash = h;
                }
                plan.vertices.reserve(static_cast<size_t>(resolved_abi.vertex_entries.GetCount()));
                for (int i = 0; i < resolved_abi.vertex_entries.GetCount(); ++i)
                    plan.vertices.push_back(resolved_abi.vertex_entries[i]);
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
            plan.manifest = ModuleResourceManifest{};
            plan.manifest_definition = definition;
            if (plan.depth_purpose)
                plan.manifest_definition.code_module_requirements.clear();
            const char *provider_root_names[2]{};
            uint32 provider_root_count = 0;
            const GLSLCodeModuleRegistry &module_registry = GetGLSLCodeModuleRegistry();
            const char *selected_provider_paths[] =
            {
                definition.fragment_material_source_module,
                plan.depth_purpose ? nullptr : definition.fragment_ntb_module
            };
            const bool include_coverage_providers =
                !plan.depth_purpose
             || plan.coverage.requires_alpha_evaluation;
            for (const char *provider_path : selected_provider_paths)
            {
                if (!include_coverage_providers)
                    break;
                if (!provider_path || !provider_path[0])
                    continue;
                const GLSLCodeModuleDefinition *provider =
                    FindSelectedProviderModule(module_registry, provider_path);
                if (!provider)
                {
                    GLogError("[ShaderGen] Selected provider has no registered metadata: %s",
                              provider_path);
                    return false;
                }
                if (provider_root_count < 2)
                    provider_root_names[provider_root_count++] = provider->name;
            }
            if (!BuildModuleResourceManifest(
                    plan.manifest_definition, plan.manifest,
                    provider_root_names, provider_root_count, &module_registry))
            {
                GLogError("[ShaderGen] Generic material resource manifest failed: name=%s",
                          definition.definition_name.c_str());
                return false;
            }
            plan.descriptors =
                BuildDescriptorsFromDefinition(definition, plan.manifest);
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
                            if (entry.set_type
                                != DescriptorSetType::Material)
                                return false;
                            if (!plan.coverage.
                                    requires_alpha_evaluation)
                                return true;

                            switch (entry.semantic)
                            {
                            case DescriptorSemantic::MaterialTexture:
                            case DescriptorSemantic::MaterialSampler:
                                return !plan.coverage.requires_texture
                                    || entry.texture_slot
                                        != plan.coverage.texture_slot;
                            case DescriptorSemantic::MaterialPrivateData:
                                return !plan.coverage.
                                    requires_material_data;
                            case DescriptorSemantic::MaterialPrivateDataIndex:
                                return !plan.effective_vertex_varying.
                                    emit_data_index_id;
                            case DescriptorSemantic::
                                MaterialTextureLayerTable:
                                return !plan.coverage.requires_texture;
                            case DescriptorSemantic::MaterialColorPalette:
                                return !plan.effective_vertex_varying.
                                    emit_vertex_color_from_palette;
                            default:
                                return true;
                            }
                        }),
                    plan.descriptors.end());
            }
            // mesh_draw_params：mesh 阶段统一 per-draw 参数表（IndirectMeshDraw——
            // mesh shader 经 gl_DrawID 查表的段偏移，替代 per-draw push constant）。
            // mesh 为唯一顶点路径，所有材质必备，不依赖材质定义/剔除规则。
            {
                SerializedDescriptorEntry mesh_params{};
                mesh_params.set_type = SBS_MeshDrawParams.set_type;
                mesh_params.stage_flags = VK_SHADER_STAGE_MESH_BIT_EXT;
                mesh_params.name = SBS_MeshDrawParams.name;
                mesh_params.struct_name = SBS_MeshDrawParams.struct_name;
                mesh_params.semantic = DescriptorSemantic::MeshDrawParams;
                mesh_params.semantic_layer = DescriptorSemanticLayer::SSBO;
                mesh_params.has_requirement_policy = true;
                mesh_params.required = true;
                mesh_params.allow_fallback = false;
                plan.descriptors.push_back(mesh_params);
            }
            if (!plan.manifest.IsValid())
            {
                GLogError("[ShaderGen] Generic material resource contract failed: name=%s error=%s",
                          definition.definition_name.c_str(),
                          GetModuleResourceManifestErrorName(plan.manifest.error));
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
            GenericMaterialBuildPlan &plan)
        {
            // Mesh shader 材质：生成 mesh stage。mesh 是唯一顶点路径。
            // 模式选择优先级：definition.mesh_shader_mode > primitive_type 推断
            const bool is_char_quad = IsCharQuadMode(definition.mesh_shader_mode);
            const bool is_lines = !is_char_quad
                && (plan.primitive_type == hgl::graph::PrimitiveType::Lines);

            MeshShaderMode ms_mode;
            uint32_t max_invocations;
            std::string input_glsl_str;
            std::string provider_glsl_str;

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
                input_glsl_str    = plan.resolved_vertex_input_glsl;
                provider_glsl_str = plan.resolved_provider_glsl;
            }
            else
            {
                ms_mode = MeshShaderMode::VertexPassthrough;
                max_invocations = ClampMeshInvocationsByDevice(
                    profile, ms_mode, kMeshVertexPassthroughMaxInvocations);
                input_glsl_str    = plan.resolved_vertex_input_glsl;
                provider_glsl_str = plan.resolved_provider_glsl;
            }

            plan.ms = GenerateMeshShader(
                plan.vertex_node_config,
                plan.varying,
                plan.position_format,
                GetShaderLibraryPath().c_str(),
                ms_mode,
                max_invocations,
                input_glsl_str,
                provider_glsl_str,
                &plan.stage_interface);

            CompositorAssembler assembler(GetShaderLibraryPath());
            MaterialOutputContractDiagnostic output_diagnostic{};
            if (!BuildMaterialOutputContract(
                    plan.purpose,
                    plan.output_contract,
                    output_diagnostic))
            {
                GLogError(
                    "[ShaderGen] Material output contract build failed: name=%s error=%s",
                    definition.definition_name.c_str(),
                    GetMaterialOutputContractErrorName(
                        output_diagnostic.error));
                return false;
            }
            CompositorAssembler::CompositorModuleOptions compositor_options{};
            compositor_options.alpha_test =
                plan.coverage.mode == MaterialCoverageMode::AlphaTest
             || plan.coverage.mode
                    == MaterialCoverageMode::AlphaTestDither;
            compositor_options.alpha_cutoff =
                plan.coverage.alpha_cutoff;
            compositor_options.dither =
                plan.coverage.mode == MaterialCoverageMode::Dither
             || plan.coverage.mode
                    == MaterialCoverageMode::AlphaTestDither;
            compositor_options.fragment_inputs = &plan.stage_interface;
            compositor_options.output_contract = &plan.output_contract;
            compositor_options.coverage_contract = &plan.coverage;
            // T3：surface → 光照管线查表（GetSurfaceLightingConfig——单一真源）
            const SurfaceLightingConfig *lighting =
                GetSurfaceLightingConfig(definition.compositor_surface);
            if (!lighting)
            {
                GLogError("[ShaderGen] Unsupported compositor surface type: %d",
                          static_cast<int>(definition.compositor_surface));
                return false;
            }
            compositor_options.enable_scene_lighting =
                lighting->enable_scene_lighting;
            compositor_options.sky_module =
                lighting->sky_module;
            compositor_options.forward_lighting_module =
                lighting->forward_lighting_module;
            compositor_options.lighting_algorithm_module =
                lighting->lighting_algorithm_module;
            compositor_options.material_source_module =
                definition.fragment_material_source_module;
            compositor_options.ntb_module =
                definition.fragment_ntb_module;

            PassType effective_pass = definition.compositor_pass;
            const char *effective_fragment_source =
                definition.fragment_source;
            if (plan.purpose
                == ShaderProgramPurpose::DepthOnly)
            {
                effective_pass = plan.coverage.requires_alpha_evaluation
                    ? PassType::EarlyZMasked
                    : PassType::EarlyZSolid;
                effective_fragment_source = nullptr;
            }
            else if (plan.purpose
                == ShaderProgramPurpose::ShadowDepth)
            {
                effective_pass = plan.coverage.requires_alpha_evaluation
                    ? PassType::ShadowMasked
                    : PassType::ShadowOpaque;
                effective_fragment_source = nullptr;
            }

            const auto assembled = assembler.Assemble(
                definition.compositor_surface,
                effective_pass,
                effective_fragment_source,
                definition.fragment_surface_module,
                compositor_options);
            if (!assembled.success)
            {
                GLogError("[ShaderGen] Generic material fragment assembly failed: name=%s error=%s",
                          definition.definition_name.c_str(),
                          assembled.error_message.c_str());
                return false;
            }
            plan.fs = assembled.fragment_glsl;
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
            CompositorMaterialBuildConfig &out_config)
        {
            out_compiler_input = MaterialShaderCompilerInput{
                definition.definition_name.c_str(),
                request.primitive_type,
                plan.vertices.data(), static_cast<uint32>(plan.vertices.size()),
                plan.descriptors.data(), static_cast<uint32>(plan.descriptors.size())
            };
            CompositorMaterialBuildConfig &config = out_config;
            config.primitive_type = request.primitive_type;
            config.shader_stage_flag_bits =
                uint32(ShaderStage::MeshFragment);
            plan.contract_definition = definition;
            plan.contract_definition.vertex_varying =
                plan.effective_vertex_varying;
            config.material_definition = &plan.contract_definition;
            config.resource_manifest = plan.manifest.IsValid() ? &plan.manifest : nullptr;
            config.merge_resource_manifest_material_slots =
                !plan.depth_purpose;
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
            const uint64 mesh_interface_hash = mesh_interface_hasher;
            const uint64 fragment_interface_hash =
                HashFinalShaderSource(plan.fs.data(), plan.fs.size());

            // mesh shader 材质：顶点阶段走 mesh stage
            plan.program_link.mesh_stage = BuildFinalShaderStageKey(
                ShaderStage::Mesh,
                plan.ms.data(),
                plan.ms.size(),
                plan.resolved_provider_graph_hash,
                mesh_interface_hash,
                resource_contract_hash,
                compiler_hash);
            plan.program_link.fragment_stage = BuildFinalShaderStageKey(
                ShaderStage::Fragment,
                plan.fs.data(),
                plan.fs.size(),
                plan.manifest.stable_hash,
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
            config.material_private_data_slot_decls =
                plan.depth_purpose
             && !plan.coverage.requires_material_data
                    ? nullptr
                    : definition.material_private_data_slot_decls.empty()
                        ? nullptr : &definition.material_private_data_slot_decls;
            config.defer_finalize = request.defer_finalize;
            return true;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════
    // BuildGenericMaterial — orchestration
    // (originally MaterialDefinitionRegistry.cpp:218-603)
    // ═══════════════════════════════════════════════════════════════════════
    ShaderBuildContext *BuildGenericMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition)
    {
        const bool semantic_contract =
            !definition.vertex_semantic_requirements.IsEmpty();
        if (!definition.fragment_source
         || !semantic_contract)
        {
            GLogError("[ShaderGen] Generic material contract invalid: name=%s fragment=%p semantic_requirements=%d",
                      definition.definition_name.c_str(),
                      definition.fragment_source,
                      definition.vertex_semantic_requirements.GetCount());
            return nullptr;
        }

        GenericMaterialBuildPlan plan{};

        // Mesh shader 材质：mesh 是唯一顶点路径（不做触发标志/分支；
        // Lines 走 LineQuad，其余走 VertexPassthrough）
        plan.primitive_type = request.primitive_type;

        if (!ResolvePurposeAndCoverage(definition, request, plan))
            return nullptr;

        if (!ResolveVertexABI(definition, request, plan))
            return nullptr;

        if (!BuildResourceContract(definition, plan))
            return nullptr;

        if (!GenerateStageSources(profile, definition, plan))
            return nullptr;

        MaterialShaderCompilerInput compiler_input{};
        CompositorMaterialBuildConfig config{};
        if (!FinalizeProgramLink(profile, definition, request, plan,
                                 compiler_input, config))
            return nullptr;

        ShaderBuildContext *result = CompileCompositorMaterial(
            profile, compiler_input,
            plan.ms,
            plan.fs, config);
        if (!result)
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        return result;
    }
}
