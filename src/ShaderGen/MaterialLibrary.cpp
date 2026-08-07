#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/log/Log.h>
#include "2d/Build2DCommon.h"
#include "3d/DefinitionDescriptorBuilder3D.h"
#include "common/VertexShaderAssembler.h"
#include "common/VertexBuilderCommon.h"
#include <vector>
#include <string>

namespace hgl::graph::mtl{
void ForceLinkPureColorMaterialDefinition();
void ForceLinkText2DMaterialDefinition();

namespace
{
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

    bool BuildResolvedVertexABI(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        std::vector<FixedVertexEntry> &out_vertices,
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

    static ShaderProgramBuildSpec *BuildGenericMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition)
    {
        const bool semantic_contract =
            !definition.vertex_semantic_requirements.IsEmpty();
        if (!definition.fragment_program_module
         || !semantic_contract)
        {
            GLogError("[ShaderGen] Generic material contract invalid: name=%s fragment=%p semantic_requirements=%d",
                      definition.definition_name.c_str(),
                      definition.fragment_program_module,
                      definition.vertex_semantic_requirements.GetCount());
            return nullptr;
        }

        VertexVaryingConfig varying{};
        varying.emit_data_index_id = definition.vertex_varying.emit_data_index_id;
        varying.emit_texture_layer_id = definition.vertex_varying.emit_texture_layer_id;
        varying.texture_layer_id_uses_data_index = definition.vertex_varying.texture_layer_id_uses_data_index;
        varying.emit_vertex_color = definition.vertex_varying.emit_vertex_color;
        varying.emit_uv0 = definition.vertex_varying.emit_uv0;
        varying.emit_world_pos = definition.vertex_varying.emit_world_pos;
        varying.emit_world_normal = definition.vertex_varying.emit_world_normal;
        varying.emit_luminance = definition.vertex_varying.emit_luminance;
        varying.emit_frag_direction = definition.vertex_varying.emit_frag_direction;
        varying.use_transform_id_attr = definition.vertex_varying.use_transform_id_attr;
        varying.emit_vertex_color_from_palette = definition.vertex_varying.emit_vertex_color_from_palette;

        std::vector<FixedVertexEntry> vertices;
        std::vector<FixedDescriptorEntry> descriptors;
        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VertexShaderNodeConfig vertex_node_config = request.recipe.vertex_node_config;
        if (request.has_transform_graph)
            vertex_node_config = request.transform_graph.ToNodeConfig();
        const bool has_explicit_recipe_node_config =
            !IsDefault3DNodeConfig(request.recipe.vertex_node_config);
        if (!request.has_transform_graph
         && !has_explicit_recipe_node_config
         && IsDefault3DNodeConfig(vertex_node_config)
         && !IsDefault3DNodeConfig(definition.vertex_node_config))
        {
            vertex_node_config = definition.vertex_node_config;
        }
        if (!request.has_transform_graph
         && !has_explicit_recipe_node_config
         && definition.has_transform_graph)
            vertex_node_config = definition.transform_graph.ToNodeConfig();
        const MaterialTransformGraph transform_graph =
            MaterialTransformGraph::FromNodeConfig(vertex_node_config);
        vertex_node_config = transform_graph.ToNodeConfig();
        std::string extra_attributes;
        std::string resolved_vertex_input_glsl;
        std::string resolved_provider_glsl;
        uint64 resolved_provider_graph_hash = 0;
        {
            MaterialResolvedVertexABI resolved_abi;
            if (!BuildResolvedMaterialVertexABI(definition, request, resolved_abi))
            {
                GLogError("[ShaderGen] Resolved vertex ABI build failed: name=%s",
                          definition.definition_name.c_str());
                return nullptr;
            }
            position_format = resolved_abi.position_format;
            resolved_vertex_input_glsl = resolved_abi.vertex_input_glsl.c_str();
            resolved_provider_glsl = resolved_abi.provider_glsl;
            resolved_provider_graph_hash = resolved_abi.provider_graph_hash;
            resolved_provider_graph_hash = hgl::hash::FNV1aAppendValueBytes(
                hgl::hash::FNV1aInit<uint64>(), transform_graph.GetHash());
            resolved_provider_graph_hash = hgl::hash::FNV1aAppendValueBytes(
                resolved_provider_graph_hash, resolved_abi.provider_graph_hash);
            vertices.reserve(static_cast<size_t>(resolved_abi.vertex_entries.GetCount()));
            for (int i = 0; i < resolved_abi.vertex_entries.GetCount(); ++i)
                vertices.push_back(resolved_abi.vertex_entries[i]);
        }
        ShaderResourceManifest manifest{};
        ShaderProgramLinkSpec resolved_program_link{};
        SkyLightAmbientModel ambient_model = request.override_sky_ambient_model
            ? request.sky_ambient_model : SkyLightAmbientModel::Simple;
        if (!request.override_sky_ambient_model)
        {
            for (const GLSLCodeModuleID id : definition.code_module_requirements)
            {
                if (id == GLSLCodeModuleID::SkyLightCubeMap)
                {
                    ambient_model = SkyLightAmbientModel::CubeMap;
                    break;
                }
            }
        }
        if (definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor)
        {
            if (!Build3DShaderResourceManifest(definition, ambient_model, manifest))
            {
                GLogError("[ShaderGen] Generic material resource manifest failed: name=%s",
                          definition.definition_name.c_str());
                return nullptr;
            }
            descriptors = Build3DDescriptorsFromDefinition(definition, manifest);
        }
        else
        {
            Material2DBuildParams params = Material2DBuildParams::From(request, definition);
            if (!build2d::Build2DShaderResourceManifest(definition, manifest))
            {
                GLogError("[ShaderGen] DirectInclude material resource manifest failed: name=%s",
                          definition.definition_name.c_str());
                return nullptr;
            }
            descriptors = build2d::Build2DDescriptorsFromDefinition(params, manifest);
        }
        std::string vs = GenerateVertexShader(vertex_node_config, varying,
                                               position_format,
                                               extra_attributes, GetShaderLibraryPath().c_str(),
                                               resolved_vertex_input_glsl,
                                               resolved_provider_glsl);

        CompositorAssembler assembler(GetShaderLibraryPath());
        CompositorAssembler::CompositorModuleOptions compositor_options{};
        compositor_options.alpha_test = request.recipe.alpha_test;
        compositor_options.alpha_cutoff = request.recipe.alpha_cutoff;
        compositor_options.dither =
            definition.compositor_blend == BlendMode::Dither
         || definition.compositor_pass == PassType::ForwardDither;
        if (definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor)
        {
            compositor_options.sky_module =
                ambient_model == SkyLightAmbientModel::CubeMap
             || ambient_model == SkyLightAmbientModel::IBL
                    ? "sky/sky_cubemap.glsl"
                    : "sky/sky_atmosphere.glsl";
        }

        const auto assembled = assembler.Assemble(
            definition.compositor_surface,
            definition.compositor_blend,
            definition.compositor_pass,
            definition.fragment_program_module,
            definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor
                ? definition.fragment_surface_module : nullptr,
            compositor_options);
        if (!assembled.success)
        {
            GLogError("[ShaderGen] Generic material fragment assembly failed: name=%s error=%s",
                      definition.definition_name.c_str(),
                      assembled.error_message.c_str());
            return nullptr;
        }
        const std::string fs = assembled.fragment_glsl;

        FixedMaterialDef fixed_definition{
            definition.definition_name.c_str(),
            request.primitive_type,
            vertices.data(), static_cast<uint32>(vertices.size()),
            descriptors.data(), static_cast<uint32>(descriptors.size())
        };
        CompositorMaterialBuildConfig config{};
        config.primitive_type = request.primitive_type;
        config.shader_stage_flag_bits = request.override_shader_stage_bits
            ? request.shader_stage_flag_bit : uint32(ShaderStage::VertexFragment);
        config.material_definition = &definition;
        config.resource_manifest = manifest.IsValid() ? &manifest : nullptr;
        config.artifact_store = request.shader_artifact_store;
        if (request.shader_artifact_store)
        {
            ShaderStageBuildSpec vertex_stage{};
            vertex_stage.stage = ShaderStage::Vertex;
            ShaderStageBuildSpec fragment_stage{};
            fragment_stage.stage = ShaderStage::Fragment;
            resolved_program_link.vertex_stage = request.enable_resolved_vertex_abi
                ? vertex_stage.BuildKeyWithProviderGraphHash(
                    resolved_provider_graph_hash)
                : vertex_stage.BuildKey();
            resolved_program_link.fragment_stage = fragment_stage.BuildKey();
            resolved_program_link.resource_layout_hash = manifest.stable_hash;
            resolved_program_link.vertex_input_hash =
                request.geometry_vertex_format
                    ? request.geometry_vertex_format->GetVertexInputHash() : 0;
            config.program_link = &resolved_program_link;
        }
        config.data_slot_decls = definition.data_slot_decls.empty()
            ? nullptr : &definition.data_slot_decls;
        ShaderProgramBuildSpec *result = CompileCompositorMaterial(profile, fixed_definition, vs, fs, config);
        if (!result)
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        return result;
    }

    struct BaseMaterialInfoRegistryEntry
    {
        bool has_preset = false;
        BuiltinMaterialCreatorID preset = BuiltinMaterialCreatorID::PureColor;
        MaterialDefinition bmi{};
    };

    std::vector<BaseMaterialInfoRegistryEntry> &GetBaseMaterialInfoRegistry()
    {
        EnsureBuiltinMaterialDefinitionsLinked();
        static std::vector<BaseMaterialInfoRegistryEntry> registry;
        return registry;
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
    std::vector<FixedVertexEntry> vertices;
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
    for (const FixedVertexEntry &entry : vertices)
        out_abi.vertex_entries.Add(entry);
    out_abi.vertex_input_glsl = vertex_input_glsl.c_str();
    if (!ComposeGLSLCodeModuleProviderGraph(resolution, out_abi.provider_glsl))
        return false;
    return true;
}

void RegisterMaterialDefinition(const MaterialDefinition &bmi)
{
    MaterialDefinition normalized = bmi;
    if (normalized.definition_id.empty())
    {
        if (!normalized.definition_name.empty())
            normalized.definition_id = normalized.definition_name;
    }

    if (normalized.definition_name.empty() && normalized.definition_id.empty())
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if ((!normalized.definition_id.empty() && entry.bmi.definition_id == normalized.definition_id)
         || (!normalized.definition_name.empty() && entry.bmi.definition_name == normalized.definition_name))
        {
            entry.bmi = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.bmi = normalized;
    registry.emplace_back(std::move(entry));
}

void RegisterMaterialDefinition(const BuiltinMaterialCreatorID preset, const MaterialDefinition &bmi)
{
    MaterialDefinition normalized = bmi;
    const char *preset_name = GetBuiltinMaterialCreatorIDName(preset);

    if (normalized.definition_id.empty())
    {
        if (preset_name && *preset_name)
            normalized.definition_id = preset_name;
    }

    if (normalized.definition_name.empty())
    {
        if (preset_name && *preset_name)
            normalized.definition_name = preset_name;
    }

    if (normalized.definition_name.empty() && normalized.definition_id.empty())
        return;

    auto &registry = GetBaseMaterialInfoRegistry();
    for (auto &entry : registry)
    {
        if (entry.has_preset && entry.preset == preset)
        {
            entry.bmi = normalized;
            return;
        }
    }

    BaseMaterialInfoRegistryEntry entry{};
    entry.has_preset = true;
    entry.preset = preset;
    entry.bmi = normalized;
    registry.emplace_back(std::move(entry));
}

bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_bmi)
{
    if (mtl_def_id.empty())
        return false;

    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.bmi.definition_id == mtl_def_id)
        {
            if (IsBootstrapMaterialDefinition(entry.bmi))
            {
                out_bmi = entry.bmi;
                return true;
            }
        }
    }

    const MaterialDefinitionFileRegistry &file_registry =
        GetMaterialDefinitionFileRegistry();
    const MaterialDefinition *file_definition =
        file_registry.FindByID(mtl_def_id.c_str());
    if (file_definition)
    {
        out_bmi = *file_definition;
        return true;
    }

    for (const auto &entry : registry)
    {
        if (entry.bmi.definition_id == mtl_def_id)
        {
            out_bmi = entry.bmi;
            return true;
        }
    }

    return false;
}


bool TryGetMaterialDefinitionByBuiltinMaterialCreatorID(const BuiltinMaterialCreatorID preset, MaterialDefinition &out_bmi)
{
    const auto &registry = GetBaseMaterialInfoRegistry();
    for (const auto &entry : registry)
    {
        if (entry.has_preset && entry.preset == preset)
        {
            out_bmi = entry.bmi;
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
            hgl::filesystem::Path(ToOSString(GetShaderLibraryPath()))
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
        registry.LoadDirectory(ToOSString(GetShaderLibraryPath()));
        loaded = true;
    }
    return registry;
}

bool MergeMaterialDefinitionFile(const MaterialDefinition &legacy,
                                 const MaterialDefinition &file,
                                 MaterialDefinition &out)
{
    if (IsBootstrapMaterialDefinition(legacy)
     || file.source_kind != MaterialDefinitionSourceKind::File)
        return false;

    out = legacy;
    out.definition_id = file.definition_id;
    out.definition_name = file.definition_name;
    out.source_kind = MaterialDefinitionSourceKind::File;
    out.usage_tag = file.usage_tag;
    out.bootstrap_kind = file.bootstrap_kind;
    out.fragment_program_mode = file.fragment_program_mode;
    out.vertex_provider_policy = file.vertex_provider_policy;
    out.vertex_semantic_requirements = file.vertex_semantic_requirements;
    out.transform_graph = file.transform_graph;
    out.has_transform_graph = file.has_transform_graph;
    out.vertex_varying = file.vertex_varying;
    out.required_ssbo_assets = file.required_ssbo_assets;
    out.data_slot_decls = file.data_slot_decls;
    out.ubo_requirements = file.ubo_requirements;
    out.texture_slot_decls = file.texture_slot_decls;
    out.code_module_requirements = file.code_module_requirements;
    out.compositor_surface = file.compositor_surface;
    out.compositor_blend = file.compositor_blend;
    out.compositor_pass = file.compositor_pass;
    out.fragment_program_module = file.fragment_program_module;
    out.fragment_surface_module = file.fragment_surface_module
        ? file.fragment_surface_module : legacy.fragment_surface_module;
    return true;
}

const char *GetBuiltinMaterialCreatorIDName(const BuiltinMaterialCreatorID mtl_id)
{
    static const char *const names[] = {
        "PureColor", "Text2D"
    };
    const uint32 index = static_cast<uint32>(mtl_id);
    return index < static_cast<uint32>(sizeof(names) / sizeof(names[0]))
        ? names[index] : nullptr;
}

ShaderProgramBuildSpec *CreateMaterialFromDefinition(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request)
{
    return BuildGenericMaterial(profile, request, definition);
}

void NormalizeRecipe(MaterialRecipe &recipe)
{
    if (recipe.mtl_def_id.empty())
        return;

    MaterialDefinition bmi{};
    const bool has_definition = TryGetMaterialDefinitionByID(recipe.mtl_def_id, bmi);
    if (has_definition)
        ApplyBaseMaterialInfoDefaults(recipe, bmi, false);

}

}//namespace hgl::graph::mtl
