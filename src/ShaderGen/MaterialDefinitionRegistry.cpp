#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include "common/GenericMaterialBuilder.h"
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/graph/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/MaterialCoverageContract.h>
#include <hgl/shadergen/ShaderBuildContext.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/log/Log.h>
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
