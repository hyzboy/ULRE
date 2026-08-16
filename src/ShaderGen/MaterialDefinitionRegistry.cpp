#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/MaterialShaderCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/MaterialOutputContract.h>
#include <hgl/shadergen/MaterialCoverageContract.h>
#include <hgl/shadergen/ShaderBuildContext.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/ShaderKeyUtility.h>
#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/log/Log.h>
#include "3d/DefinitionDescriptorBuilder3D.h"
#include "common/VertexShaderAssembler.h"
#include "common/VertexBuilderCommon.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>

namespace hgl::graph::mtl{
using namespace hgl::graph::shadergen;
void ForceLinkPureColorMaterialDefinition();
void ForceLinkText2DMaterialDefinition();

namespace
{
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

    const char *GetVertexInputName(const VertexSemantic semantic)
    {
        switch (semantic)
        {
        case VertexSemantic::Position:    return "Position";
        case VertexSemantic::Normal:      return "Normal";
        case VertexSemantic::Tangent:     return "Tangent";
        case VertexSemantic::Bitangent:   return "Binormal";
        case VertexSemantic::Color:       return "Color";
        case VertexSemantic::Luminance:   return "Luminance";
        case VertexSemantic::TexCoord:    return "TexCoord";
        case VertexSemantic::TransformID: return "TransformID";
        default:                          return nullptr;
        }
    }

    const char *GetGLSLVertexInputType(const VkFormat format,
                                       const uint8 component_count)
    {
        const uint32 numeric_class =
            GLSLCodeModuleCapabilityResolver::GetNumericClassFromVkFormat(format);
        if (numeric_class == 0 || component_count == 0 || component_count > 4)
            return nullptr;

        const bool is_signed_integer = numeric_class
            & uint32(GLSLCodeModuleNumericClass::SignedInteger);
        const bool is_unsigned_integer = numeric_class
            & uint32(GLSLCodeModuleNumericClass::UnsignedInteger);
        if (component_count == 1)
            return is_signed_integer ? "int" : is_unsigned_integer ? "uint" : "float";

        if (is_signed_integer)
        {
            static const char *const types[] = {nullptr, nullptr, "ivec2", "ivec3", "ivec4"};
            return types[component_count];
        }

        if (is_unsigned_integer)
        {
            static const char *const types[] = {nullptr, nullptr, "uvec2", "uvec3", "uvec4"};
            return types[component_count];
        }

        static const char *const types[] = {nullptr, nullptr, "vec2", "vec3", "vec4"};
        return types[component_count];
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

    bool BuildResolvedVertexABI(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        std::vector<SerializedVertexEntry> &out_vertices,
        VkFormat &out_position_format,
        std::string &out_vertex_input_glsl,
        GLSLCodeModuleResolutionResult &out_resolution)
    {
        switch (definition.vertex_provider_policy)
        {
        case MaterialVertexProviderPolicy::Auto:
        case MaterialVertexProviderPolicy::GeometryOnly:
        case MaterialVertexProviderPolicy::AllowDerived:
            break;
        default:
            return false;
        }
        if (!request.geometry_vertex_format)
            return false;

        if (request.vertex_code_module_registry)
        {
            if (!PreviewMaterialVertexSemanticResolution(
                    *request.vertex_code_module_registry, definition, request, out_resolution)
             || !out_resolution.resolved)
                return false;
        }
        else
        {
            out_resolution = GLSLCodeModuleResolutionResult{};
            out_resolution.resolved = true;
        }

        const GeometryVertexFormat &geometry = *request.geometry_vertex_format;
        out_vertices.clear();
        out_vertex_input_glsl.clear();
        out_position_format = VK_FORMAT_UNDEFINED;

        for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
        {
            const auto &requirement = definition.vertex_semantic_requirements[i];
            const VertexSemantic semantic =
                GetVertexSemanticFromGLSLCodeModuleSemantic(requirement.semantic);
            const char *name = GetVertexInputName(semantic);
            if (semantic == VertexSemantic::Color
             && definition.vertex_varying.emit_vertex_color_from_palette)
                name = "ColorIndex";
            const GeometryVertexAttributeFormat *attribute = geometry.Find(semantic);
            if (!name || !attribute)
                return false;

            int location = -1;
            for (uint32 index = 0; index < geometry.GetCount(); ++index)
            {
                if (geometry.Get(index) == attribute)
                {
                    location = static_cast<int>(index);
                    break;
                }
            }
            const char *const type = GetGLSLVertexInputType(
                attribute->format, attribute->vec_size);
            if (location < 0 || !type)
                return false;

            out_vertices.push_back({attribute->format, semantic});
            out_vertex_input_glsl += "layout(location=" + std::to_string(location)
                + ") in " + type + " " + name + ";\n";
            if (semantic == VertexSemantic::Position)
                out_position_format = attribute->format;
        }

        return out_position_format != VK_FORMAT_UNDEFINED;
    }

    void EnsureBuiltinMaterialDefinitionsLinked()
    {
        static const bool linked = []() -> bool
        {
            ForceLinkPureColorMaterialDefinition();
            ForceLinkText2DMaterialDefinition();
            return true;
        }();
        (void)linked;
    }

    static ShaderBuildContext *BuildGenericMaterial(
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

        const ResolvedMaterialRenderState render_state =
            ResolveMaterialRenderState(definition, request.recipe);
        const ShaderProgramPurpose shader_program_purpose =
            request.override_shader_program_purpose
                ? request.shader_program_purpose
                : GetShaderProgramPurpose(
                    definition.compositor_pass);
        MaterialCoverageContract coverage_contract{};
        if (!BuildMaterialCoverageContract(
                definition,
                request.recipe,
                shader_program_purpose,
                coverage_contract))
            return nullptr;
        const bool depth_purpose =
            shader_program_purpose == ShaderProgramPurpose::DepthOnly
         || shader_program_purpose == ShaderProgramPurpose::ShadowDepth;

        const MaterialVertexVaryingConfig effective_vertex_varying =
            ResolveMaterialVertexVaryingConfig(
                definition,
                shader_program_purpose,
                coverage_contract);
        MaterialDefinition vertex_definition = definition;
        vertex_definition.vertex_varying = effective_vertex_varying;
        if (depth_purpose)
        {
            vertex_definition.vertex_semantic_requirements.Clear();
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
                        semantic, effective_vertex_varying))
                {
                    vertex_definition.vertex_semantic_requirements.Add(
                        requirement);
                }
            }
        }

        VertexVaryingConfig varying{};
        varying.emit_data_index_id = effective_vertex_varying.emit_data_index_id;
        varying.emit_vertex_color = effective_vertex_varying.emit_vertex_color;
        varying.emit_uv0 = effective_vertex_varying.emit_uv0;
        varying.emit_world_pos = effective_vertex_varying.emit_world_pos;
        varying.emit_world_normal = effective_vertex_varying.emit_world_normal;
        varying.emit_luminance = effective_vertex_varying.emit_luminance;
        varying.emit_frag_direction = effective_vertex_varying.emit_frag_direction;
        varying.use_transform_id_attr = effective_vertex_varying.use_transform_id_attr;
        varying.emit_vertex_color_from_palette = effective_vertex_varying.emit_vertex_color_from_palette;

        ValueArray<InterStageSemanticContractEntry> stage_interface;
        MaterialStageInterfaceDiagnostic stage_interface_diagnostic{};
        if (!BuildMaterialStageInterface(
                effective_vertex_varying,
                stage_interface,
                stage_interface_diagnostic))
        {
            GLogError(
                "[ShaderGen] Material stage interface build failed: name=%s error=%s",
                definition.definition_name.c_str(),
                GetMaterialStageInterfaceErrorName(
                    stage_interface_diagnostic.error));
            return nullptr;
        }

        std::vector<SerializedVertexEntry> vertices;
        std::vector<SerializedDescriptorEntry> descriptors;
        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VertexShaderNodeConfig vertex_node_config =
            ResolveMaterialVertexNodeConfig(definition, request);
        std::string extra_attributes;
        std::string resolved_vertex_input_glsl;
        std::string resolved_provider_glsl;
        uint64 resolved_provider_graph_hash = 0;
        {
            MaterialResolvedVertexABI resolved_abi;
            if (!BuildResolvedMaterialVertexABI(
                    vertex_definition, request, resolved_abi))
            {
                GLogError("[ShaderGen] Resolved vertex ABI build failed: name=%s",
                          definition.definition_name.c_str());
                return nullptr;
            }
            position_format = resolved_abi.position_format;
            resolved_vertex_input_glsl = resolved_abi.vertex_input_glsl.c_str();
            resolved_provider_glsl = resolved_abi.provider_glsl;
            resolved_provider_graph_hash = resolved_abi.provider_graph_hash;
            {
                hgl::hash::FNV1aHasher64 h;
                h << VertexNodeConfigResolver::GetHash(vertex_node_config)
                  << resolved_abi.provider_graph_hash;
                resolved_provider_graph_hash = h;
            }
            vertices.reserve(static_cast<size_t>(resolved_abi.vertex_entries.GetCount()));
            for (int i = 0; i < resolved_abi.vertex_entries.GetCount(); ++i)
                vertices.push_back(resolved_abi.vertex_entries[i]);
        }
        ShaderResourceManifest manifest{};
        MaterialDefinition manifest_definition = definition;
        if (depth_purpose)
            manifest_definition.code_module_requirements.clear();
        GLSLCodeModuleID provider_roots[2]{};
        uint32 provider_root_count = 0;
        const GLSLCodeModuleRegistry &module_registry = GetGLSLCodeModuleRegistry();
        const char *selected_provider_paths[] =
        {
            definition.fragment_material_source_module,
            depth_purpose ? nullptr : definition.fragment_ntb_module
        };
        const bool include_coverage_providers =
            !depth_purpose
         || coverage_contract.requires_alpha_evaluation;
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
                return nullptr;
            }
            if (provider_root_count < 2)
                provider_roots[provider_root_count++] = provider->id;
        }
        ShaderLinkSpec resolved_program_link{};
        if (!Build3DShaderResourceManifest(
                manifest_definition, manifest,
                provider_roots, provider_root_count, &module_registry))
        {
            GLogError("[ShaderGen] Generic material resource manifest failed: name=%s",
                      definition.definition_name.c_str());
            return nullptr;
        }
        descriptors = Build3DDescriptorsFromDefinition(definition, manifest);
        if (depth_purpose)
        {
            descriptors.erase(
                std::remove_if(
                    descriptors.begin(),
                    descriptors.end(),
                    [&](const SerializedDescriptorEntry &entry)
                    {
                        if (entry.semantic == DescriptorSemantic::SkyInfo)
                            return true;
                        if (entry.set_type
                            != DescriptorSetType::Material)
                            return false;
                        if (!coverage_contract.
                                requires_alpha_evaluation)
                            return true;

                        switch (entry.semantic)
                        {
                        case DescriptorSemantic::MaterialTexture:
                        case DescriptorSemantic::MaterialSampler:
                            return !coverage_contract.requires_texture
                                || entry.texture_slot
                                    != coverage_contract.texture_slot;
                        case DescriptorSemantic::MaterialDataSlotData:
                            return !coverage_contract.
                                requires_material_data;
                        case DescriptorSemantic::MaterialDataIndexTable:
                            return !effective_vertex_varying.
                                emit_data_index_id;
                        case DescriptorSemantic::
                            MaterialTextureLayerTable:
                            return !coverage_contract.requires_texture;
                        case DescriptorSemantic::MaterialColorPalette:
                            return !effective_vertex_varying.
                                emit_vertex_color_from_palette;
                        default:
                            return true;
                        }
                    }),
                descriptors.end());
        }
        if (!manifest.IsValid())
        {
            GLogError("[ShaderGen] Generic material resource contract failed: name=%s error=%s",
                      definition.definition_name.c_str(),
                      GetShaderResourceManifestErrorName(manifest.error));
            return nullptr;
        }
        DescriptorContract descriptor_contract{};
        if (!BuildDescriptorContract(
                descriptors, descriptor_contract))
        {
            GLogError(
                "[ShaderGen] Material descriptor contract build failed: name=%s",
                definition.definition_name.c_str());
            return nullptr;
        }
        std::string vs = GenerateVertexShader(vertex_node_config, varying,
                                               position_format,
                                               extra_attributes, GetShaderLibraryPath().c_str(),
                                               resolved_vertex_input_glsl,
                                               resolved_provider_glsl,
                                               &stage_interface);

        CompositorAssembler assembler(GetShaderLibraryPath());
        OutputContract output_contract{};
        MaterialOutputContractDiagnostic output_diagnostic{};
        if (!BuildMaterialOutputContract(
                shader_program_purpose,
                output_contract,
                output_diagnostic))
        {
            GLogError(
                "[ShaderGen] Material output contract build failed: name=%s error=%s",
                definition.definition_name.c_str(),
                GetMaterialOutputContractErrorName(
                    output_diagnostic.error));
            return nullptr;
        }
        CompositorAssembler::CompositorModuleOptions compositor_options{};
        compositor_options.alpha_test =
            coverage_contract.mode == MaterialCoverageMode::AlphaTest
         || coverage_contract.mode
                == MaterialCoverageMode::AlphaTestDither;
        compositor_options.alpha_cutoff =
            coverage_contract.alpha_cutoff;
        compositor_options.dither =
            coverage_contract.mode == MaterialCoverageMode::Dither
         || coverage_contract.mode
                == MaterialCoverageMode::AlphaTestDither;
        compositor_options.use_resolved_render_state = true;
        compositor_options.fragment_inputs = &stage_interface;
        compositor_options.output_contract = &output_contract;
        compositor_options.coverage_contract = &coverage_contract;
        const bool use_scene_lighting =
            definition.compositor_surface != SurfaceType::Unlit
         && definition.compositor_surface != SurfaceType::Sky;
        compositor_options.enable_scene_lighting =
            use_scene_lighting;
        compositor_options.sky_module = use_scene_lighting
            ? "sky/sky_atmosphere.glsl"
            : nullptr;
        compositor_options.forward_lighting_module =
            use_scene_lighting
                ? "compositor/forward_lighting.glsl"
                : "compositor/flat_lighting.glsl";
        compositor_options.lighting_algorithm_module =
            use_scene_lighting
                ? "lighting/forward_pbr.glsl"
                : "lighting/forward_flat.glsl";
        compositor_options.material_source_module =
            definition.fragment_material_source_module;
        compositor_options.ntb_module =
            definition.fragment_ntb_module;

        PassType effective_pass = definition.compositor_pass;
        const char *effective_fragment_source =
            definition.fragment_source;
        if (shader_program_purpose
            == ShaderProgramPurpose::DepthOnly)
        {
            effective_pass = coverage_contract.requires_alpha_evaluation
                ? PassType::EarlyZMasked
                : PassType::EarlyZSolid;
            effective_fragment_source = nullptr;
        }
        else if (shader_program_purpose
            == ShaderProgramPurpose::ShadowDepth)
        {
            effective_pass = coverage_contract.requires_alpha_evaluation
                ? PassType::ShadowMasked
                : PassType::ShadowOpaque;
            effective_fragment_source = nullptr;
        }

        const auto assembled = assembler.Assemble(
            definition.compositor_surface,
            definition.compositor_blend,
            effective_pass,
            effective_fragment_source,
            definition.fragment_surface_module,
            compositor_options);
        if (!assembled.success)
        {
            GLogError("[ShaderGen] Generic material fragment assembly failed: name=%s error=%s",
                      definition.definition_name.c_str(),
                      assembled.error_message.c_str());
            return nullptr;
        }
        const std::string fs = assembled.fragment_glsl;

        MaterialShaderCompilerInput compiler_input{
            definition.definition_name.c_str(),
            request.primitive_type,
            vertices.data(), static_cast<uint32>(vertices.size()),
            descriptors.data(), static_cast<uint32>(descriptors.size())
        };
        CompositorMaterialBuildConfig config{};
        config.primitive_type = request.primitive_type;
        config.shader_stage_flag_bits =
            uint32(ShaderStage::VertexFragment);
        MaterialDefinition contract_definition = definition;
        contract_definition.vertex_varying =
            effective_vertex_varying;
        config.material_definition = &contract_definition;
        config.resource_manifest = manifest.IsValid() ? &manifest : nullptr;
        config.merge_resource_manifest_material_slots =
            !depth_purpose;
        config.artifact_store = request.shader_artifact_store;
        config.descriptor_contract = &descriptor_contract;
        const uint64 resource_contract_hash =
            GetDescriptorContractHash(
                descriptor_contract,
                depth_purpose ? 0 : manifest.stable_hash);
        const uint64 vertex_input_hash = request.geometry_vertex_format
            ? request.geometry_vertex_format->GetVertexInputHash() : 0;
        const uint64 compiler_hash =
            contract::GetShaderCompilerProfileHash(profile);
        hgl::hash::FNV1aHasher64 vertex_interface_hasher;
        vertex_interface_hasher << HashFinalShaderSource(vs.data(), vs.size())
                                << vertex_input_hash;
        const uint64 vertex_interface_hash = vertex_interface_hasher;
        const uint64 fragment_interface_hash =
            HashFinalShaderSource(fs.data(), fs.size());

        resolved_program_link.vertex_stage = BuildFinalShaderStageKey(
            ShaderStage::Vertex,
            vs.data(),
            vs.size(),
            resolved_provider_graph_hash,
            vertex_interface_hash,
            resource_contract_hash,
            compiler_hash);
        resolved_program_link.fragment_stage = BuildFinalShaderStageKey(
            ShaderStage::Fragment,
            fs.data(),
            fs.size(),
            manifest.stable_hash,
            fragment_interface_hash,
            resource_contract_hash,
            compiler_hash);
        resolved_program_link.resource_layout_hash =
            resource_contract_hash;
        resolved_program_link.vertex_input_hash = vertex_input_hash;
        resolved_program_link.render_target_hash =
            GetOutputContractHash(output_contract);
        resolved_program_link.compiler_hash = compiler_hash;
        config.program_link = &resolved_program_link;
        config.data_slot_decls =
            depth_purpose
         && !coverage_contract.requires_material_data
                ? nullptr
                : definition.data_slot_decls.empty()
                    ? nullptr : &definition.data_slot_decls;
        config.generate_only = request.generate_only;
        ShaderBuildContext *result = CompileCompositorMaterial(
            profile, compiler_input, vs, fs, config);
        if (!result)
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        return result;
    }

    struct BaseMaterialInfoRegistryEntry
    {
        bool has_preset = false;
        MaterialDefinitionBootstrapKind preset = MaterialDefinitionBootstrapKind::None;
        MaterialDefinition definition{};
    };

    struct MaterialDefinitionAliasRegistryEntry
    {
        AnsiString alias_id;
        AnsiString definition_id;
    };

    std::vector<BaseMaterialInfoRegistryEntry> &GetBaseMaterialInfoRegistry()
    {
        EnsureBuiltinMaterialDefinitionsLinked();
        static std::vector<BaseMaterialInfoRegistryEntry> registry;
        return registry;
    }

    ManagedArray<MaterialDefinitionAliasRegistryEntry> &GetMaterialDefinitionAliasRegistry()
    {
        static ManagedArray<MaterialDefinitionAliasRegistryEntry> registry;
        return registry;
    }

    const AnsiString *FindMaterialDefinitionAlias(const char *alias_id)
    {
        if (!alias_id || !alias_id[0])
            return nullptr;

        auto &registry = GetMaterialDefinitionAliasRegistry();
        for (int i = 0; i < registry.GetCount(); ++i)
        {
            if (std::strcmp(registry[i]->alias_id.c_str(), alias_id) == 0)
                return &registry[i]->definition_id;
        }
        return nullptr;
    }

    bool TryGetMaterialDefinitionByIDInternal(
        const char *mtl_def_id,
        MaterialDefinition &out_definition,
        const uint32 alias_depth)
    {
        if (!mtl_def_id || !mtl_def_id[0] || alias_depth > 8)
            return false;

        const auto &registry = GetBaseMaterialInfoRegistry();

        // Bootstrap definitions are the only creator-backed runtime
        // definitions. They are checked before files so a fallback cannot be
        // replaced by a same-named TOML definition.
        for (const auto &entry : registry)
        {
            if (entry.definition.definition_id == mtl_def_id
             && IsBootstrapMaterialDefinition(entry.definition))
            {
                out_definition = entry.definition;
                return true;
            }
        }

        // Ordinary material identity is file-backed. A file lookup is exact;
        // Registered aliases are considered only after canonical IDs.
        const MaterialDefinitionFileRegistry &file_registry =
            GetMaterialDefinitionFileRegistry();
        const MaterialDefinition *file_definition =
            file_registry.FindByID(mtl_def_id);
        if (file_definition)
        {
            out_definition = *file_definition;
            return true;
        }

        const AnsiString *canonical_id =
            FindMaterialDefinitionAlias(mtl_def_id);
        if (canonical_id
         && std::strcmp(canonical_id->c_str(), mtl_def_id) != 0)
            return TryGetMaterialDefinitionByIDInternal(
                canonical_id->c_str(), out_definition, alias_depth + 1);

        return false;
    }
}

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format)
{
    if(!gvf)
        return fallback_format;

    const GeometryVertexAttributeFormat *attribute=gvf->Find(semantic);
    if(!attribute||attribute->format==VK_FORMAT_UNDEFINED)
        return fallback_format;

    return attribute->format;
}

VertexShaderNodeConfig ResolveMaterialVertexNodeConfig(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request) noexcept
{
    // 1. request 显式覆盖
    if (request.has_vertex_node_config_override)
        return request.vertex_node_config_override;

    // 2. recipe 显式设置
    if (!IsDefault3DNodeConfig(request.recipe.vertex_node_config))
        return request.recipe.vertex_node_config;

    // 3. definition 非默认
    if (!IsDefault3DNodeConfig(definition.vertex_node_config))
        return definition.vertex_node_config;

    // 4. fallback
    return request.recipe.vertex_node_config;
}

uint64 HashMaterialProgramBuildContext(
    const PrimitiveType primitive_type,
    const GeometryVertexFormat *geometry_vertex_format,
    const contract::PhysicalDeviceProfileLite *profile) noexcept
{
    hgl::hash::FNV1aHasher64 h;

    h << primitive_type
      << (geometry_vertex_format
            ? geometry_vertex_format->GetVertexInputHash() : 0)
      << contract::GetPhysicalDeviceProfileHash(profile);
    return h;
}

MaterialVertexVaryingConfig ResolveMaterialVertexVaryingConfig(
    const MaterialDefinition &definition,
    const shadergen::ShaderProgramPurpose purpose,
    const shadergen::MaterialCoverageContract &coverage) noexcept
{
    MaterialVertexVaryingConfig varying =
        definition.vertex_varying;
    const bool depth_purpose =
        purpose == ShaderProgramPurpose::DepthOnly
     || purpose == ShaderProgramPurpose::ShadowDepth;
    if (!depth_purpose)
        return varying;

    varying.emit_world_pos = false;
    varying.emit_world_normal = false;
    varying.emit_frag_direction = false;
    varying.emit_data_index_id = false;
    varying.emit_vertex_color = false;
    varying.emit_uv0 = false;
    varying.emit_luminance = false;
    varying.emit_vertex_color_from_palette = false;

    if (!coverage.requires_alpha_evaluation)
        return varying;

    const auto needs_semantic =
        [&coverage](const InterStageSemantic semantic)
    {
        return (coverage.required_semantics
            & GetInterStageSemanticMask(semantic)) != 0;
    };
    varying.emit_data_index_id =
        needs_semantic(InterStageSemantic::DataIndexID);
    varying.emit_vertex_color =
        needs_semantic(InterStageSemantic::Color)
     && definition.vertex_varying.emit_vertex_color;
    varying.emit_vertex_color_from_palette =
        needs_semantic(InterStageSemantic::Color)
     && definition.vertex_varying.
            emit_vertex_color_from_palette;
    varying.emit_uv0 =
        needs_semantic(InterStageSemantic::UV0);
    varying.emit_luminance =
        needs_semantic(InterStageSemantic::Luminance);
    return varying;
}

bool PreviewMaterialVertexSemanticResolution(
    const GLSLCodeModuleRegistry &registry,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    GLSLCodeModuleResolutionResult &out_result)
{
    out_result.resolved = false;
    out_result.selections.Clear();
    out_result.diagnostics.Clear();

    if (definition.vertex_semantic_requirements.IsEmpty()
     || !request.geometry_vertex_format)
        return false;

    ValueArray<GLSLCodeModuleGeometryCapability> geometry_capabilities;
    if (!GLSLCodeModuleCapabilityResolver::BuildGeometryCapabilities(
            *request.geometry_vertex_format, geometry_capabilities))
        return false;

    const GLSLCodeModuleResolutionRequest resolution_request{
        definition.vertex_semantic_requirements.GetData(),
        static_cast<uint32>(definition.vertex_semantic_requirements.GetCount()),
        geometry_capabilities.GetData(),
        static_cast<uint32>(geometry_capabilities.GetCount()),
        nullptr,
        0,
        nullptr,
        0
    };
    GLSLCodeModuleCapabilityResolver resolver;
    resolver.Resolve(registry, resolution_request, out_result);
    return true;
}

bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi)
{
    std::vector<SerializedVertexEntry> vertices;
    std::string vertex_input_glsl;
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    GLSLCodeModuleResolutionResult resolution;
    if (!BuildResolvedVertexABI(definition, request, vertices, position_format,
                                vertex_input_glsl, resolution))
        return false;

    out_abi.position_format = position_format;
    out_abi.provider_graph_hash =
        GetGLSLCodeModuleProviderGraphHash(resolution);
    out_abi.vertex_entries.Clear();
    for (const SerializedVertexEntry &entry : vertices)
        out_abi.vertex_entries.Add(entry);
    out_abi.vertex_input_glsl = vertex_input_glsl.c_str();
    if (!ComposeGLSLCodeModuleProviderGraph(resolution, out_abi.provider_glsl))
        return false;
    return true;
}

void RegisterMaterialDefinition(const MaterialDefinition &definition)
{
    MaterialDefinition normalized = definition;
    if (normalized.definition_id.empty()
     || normalized.source_kind != MaterialDefinitionSourceKind::BuiltIn
     || !IsBootstrapMaterialDefinition(normalized))
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if (entry.definition.definition_id == normalized.definition_id)
        {
            entry.has_preset = true;
            entry.preset = normalized.bootstrap_kind;
            entry.definition = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.has_preset = true;
    entry.preset = normalized.bootstrap_kind;
    entry.definition = normalized;
    registry.emplace_back(std::move(entry));
}

void RegisterMaterialDefinitionAlias(const char *alias_id, const char *definition_id)
{
    if (!alias_id || !alias_id[0]
     || !definition_id || !definition_id[0]
     || std::strcmp(alias_id, definition_id) == 0)
        return;

    const auto &definitions = GetBaseMaterialInfoRegistry();
    for (const auto &entry : definitions)
    {
        if (entry.definition.definition_id == alias_id)
            return;
    }

    auto &aliases = GetMaterialDefinitionAliasRegistry();
    for (int i = 0; i < aliases.GetCount(); ++i)
    {
        if (std::strcmp(aliases[i]->alias_id.c_str(), alias_id) == 0)
            return;
    }

    MaterialDefinitionAliasRegistryEntry *entry = aliases.Create();
    if (!entry)
        return;
    entry->alias_id = alias_id;
    entry->definition_id = definition_id;
}

bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition)
{
    return TryGetMaterialDefinitionByIDInternal(mtl_def_id.c_str(), out_definition, 0);
}


bool TryGetMaterialDefinitionByBootstrapKind(const MaterialDefinitionBootstrapKind kind, MaterialDefinition &out_definition)
{
    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.has_preset
         && entry.preset == kind
         && IsBootstrapMaterialDefinition(entry.definition))
        {
            out_definition = entry.definition;
            return true;
        }
    }

    return false;
}

MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry()
{
    static MaterialDefinitionFileRegistry registry;
    static bool loaded = false;
    if (!loaded)
    {
        int file_count = 0;
        int error_count = 0;
        const hgl::filesystem::Path material_path =
            hgl::filesystem::Path(ToOSString(shadergen::GetShaderLibraryPath()))
            / OSString(OS_TEXT("material"));
        if (!registry.LoadDirectory(
                material_path.ToOSString(), &file_count, &error_count))
        {
            GLogWarning("[ShaderGen] Material TOML directory unavailable; using built-in definitions");
        }
        else
        {
            GLogInfo("[ShaderGen] Loaded %d material TOML definitions (%d errors)",
                     file_count, error_count);
        }
        loaded = true;
    }
    return registry;
}

GLSLCodeModuleRegistry &GetGLSLCodeModuleRegistry()
{
    static GLSLCodeModuleRegistry registry;
    static bool loaded = false;
    if (!loaded)
    {
        registry.RegisterBuiltinModules();
        registry.LoadDirectory(ToOSString(shadergen::GetShaderLibraryPath()));
        loaded = true;
    }
    return registry;
}

shadergen::ShaderBuildContext *CreateMaterialFromDefinition(
    const shadergen::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request)
{
    MaterialDefinition canonical_definition = definition;

    ShaderBuildContext *result =
        BuildGenericMaterial(profile, request, canonical_definition);
    return result;
}

void NormalizeRecipe(MaterialRecipe &recipe)
{
    if (recipe.mtl_def_id.empty())
        return;

    MaterialDefinition definition{};
    bool has_definition = TryGetMaterialDefinitionByID(recipe.mtl_def_id, definition);
    if (has_definition)
    {
        // Aliases are accepted only at the compatibility boundary. Once a
        // recipe is normalized, the canonical definition ID is the sole
        // runtime identity used by hashing and caches.
        recipe.mtl_def_id = definition.definition_id;
        ApplyBaseMaterialInfoDefaults(recipe, definition, false);

        const ResolvedMaterialRenderState resolved =
            ResolveMaterialRenderState(definition, recipe);

        // Write resolved values back to render_state_overrides as authoritative.
        recipe.render_state_overrides.has_double_sided = true;
        recipe.render_state_overrides.double_sided = resolved.double_sided;
        recipe.render_state_overrides.has_alpha_test = true;
        recipe.render_state_overrides.alpha_test = resolved.alpha_test;
        recipe.render_state_overrides.has_alpha_cutoff = true;
        recipe.render_state_overrides.alpha_cutoff = resolved.alpha_cutoff;
        recipe.render_state_overrides.has_dither = true;
        recipe.render_state_overrides.dither = resolved.dither;
        recipe.render_state_overrides.has_pipeline_config = true;
        recipe.render_state_overrides.pipeline_config = resolved.pipeline_config;
    }

}

}//namespace hgl::graph::mtl
