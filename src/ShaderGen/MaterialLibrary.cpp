#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/log/Log.h>
#include "2d/Build2DCommon.h"
#include "3d/DefinitionDescriptorBuilder3D.h"
#include "common/VertexShaderAssembler.h"
#include "common/VertexBuilderCommon.h"
#include <vector>
#include <string>

namespace hgl::graph::mtl{
void ForceLinkVertexColor2DMaterialDefinition();
void ForceLinkPureColor2DMaterialDefinition();
void ForceLinkPureTexture2DMaterialDefinition();
void ForceLinkRectTexture2DMaterialDefinition();
void ForceLinkRectTexture2DArrayMaterialDefinition();
void ForceLinkText2DMaterialDefinition();
void ForceLinkPureColor3DMaterialDefinition();
void ForceLinkVertexColor3DMaterialDefinition();
void ForceLinkVertexLuminance3DMaterialDefinition();
void ForceLinkVertexPattleColor3DMaterialDefinition();
void ForceLinkGizmo3DMaterialDefinition();
void ForceLinkSkyMinimalMaterialDefinition();
void ForceLinkStandardMaterialDefinition();
void ForceLinkStandardTextureArrayMaterialDefinition();

namespace
{
    void EnsureBuiltinMaterialDefinitionsLinked()
    {
        static const bool linked = []() -> bool
        {
            ForceLinkVertexColor2DMaterialDefinition();
            ForceLinkPureColor2DMaterialDefinition();
            ForceLinkPureTexture2DMaterialDefinition();
            ForceLinkRectTexture2DMaterialDefinition();
            ForceLinkRectTexture2DArrayMaterialDefinition();
            ForceLinkText2DMaterialDefinition();
            ForceLinkPureColor3DMaterialDefinition();
            ForceLinkVertexColor3DMaterialDefinition();
            ForceLinkVertexLuminance3DMaterialDefinition();
            ForceLinkVertexPattleColor3DMaterialDefinition();
            ForceLinkGizmo3DMaterialDefinition();
            ForceLinkSkyMinimalMaterialDefinition();
            ForceLinkStandardMaterialDefinition();
            ForceLinkStandardTextureArrayMaterialDefinition();
            return true;
        }();
        (void)linked;
    }

    static ShaderProgramBuildSpec *BuildGenericMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition)
    {
        if (!definition.fragment_program_module
         || definition.vertex_stage.stage != ShaderStage::Vertex
         || definition.fragment_stage.stage != ShaderStage::Fragment
         || definition.vertex_attributes.IsEmpty())
        {
            GLogError("[ShaderGen] Generic material contract invalid: name=%s fragment=%p vertex_stage=%u fragment_stage=%u attributes=%d",
                      definition.definition_name.c_str(),
                      definition.fragment_program_module,
                      static_cast<uint32>(definition.vertex_stage.stage),
                      static_cast<uint32>(definition.fragment_stage.stage),
                      definition.vertex_attributes.GetCount());
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
        varying.emit_vertex_color_from_pattle = definition.vertex_varying.emit_vertex_color_from_pattle;

        std::vector<FixedVertexEntry> vertices;
        std::vector<FixedDescriptorEntry> descriptors;
        const VkFormat position_format = ResolveMaterialVertexSemanticFormat(
            request.geometry_vertex_format,
            definition.vertex_attributes[0].semantic,
            definition.vertex_attributes[0].format);
        VertexShaderNodeConfig vertex_node_config = request.recipe.vertex_node_config;
        if (IsDefault3DNodeConfig(vertex_node_config)
         && !IsDefault3DNodeConfig(definition.vertex_node_config))
        {
            vertex_node_config = definition.vertex_node_config;
        }
        vertex_builder_common::VertexSemanticDecl declarations[8]{};
        uint32 declaration_count = 0;
        std::string extra_attributes;
        for (int i = 0; i < definition.vertex_attributes.GetCount() && declaration_count < 8; ++i)
        {
            const MaterialVertexAttributeDefinition &attribute = definition.vertex_attributes[i];
            const VkFormat resolved_format = ResolveMaterialVertexSemanticFormat(
                request.geometry_vertex_format,
                attribute.semantic,
                attribute.format);
            declarations[declaration_count++] = {attribute.semantic, resolved_format};
            if (attribute.glsl_declaration)
                extra_attributes += attribute.glsl_declaration;
        }
        const vertex_builder_common::VertexBuildInput input{
            request.primitive_type, request.geometry_vertex_format,
            declarations, declaration_count
        };
        vertices = vertex_builder_common::BuildVertexEntries(input);
        ShaderResourceManifest manifest{};
        if (definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor)
        {
            if (!Build3DShaderResourceManifest(definition, request.override_sky_ambient_model
                ? request.sky_ambient_model : SkyLightAmbientModel::Simple, manifest))
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
            build2d::PushBaseDescriptorEntries(descriptors, params);
        }
        std::string vs = GenerateVertexShader(vertex_node_config, varying,
                                               position_format,
                                               extra_attributes, "ShaderLibrary");

        std::string fs;
        if (definition.fragment_program_mode == MaterialFragmentProgramMode::Compositor)
        {
            CompositorAssembler assembler("ShaderLibrary");
            const auto assembled = assembler.Assemble(
                definition.compositor_surface,
                definition.compositor_blend,
                definition.compositor_pass,
                definition.fragment_program_module,
                definition.fragment_surface_module);
            if (!assembled.success)
            {
                GLogError("[ShaderGen] Generic material compositor assembly failed: name=%s error=%s",
                          definition.definition_name.c_str(),
                          assembled.error_message.c_str());
                return nullptr;
            }
            fs = assembled.fragment_glsl;
        }
        else
        {
            fs = "#version 450\n#include \"" + std::string(definition.fragment_program_module) + "\"\n";
        }

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
        config.geometry_vertex_format = request.geometry_vertex_format;
        config.material_definition = &definition;
        config.resource_manifest = manifest.IsValid() ? &manifest : nullptr;
        config.ssbo_slot_decls = definition.ssbo_slot_decls.empty()
            ? nullptr : &definition.ssbo_slot_decls;
        ShaderProgramBuildSpec *result = CompileCompositorMaterial(profile, fixed_definition, vs, fs, config);
        if (!result)
            GLogError("[ShaderGen] Generic material compilation failed: name=%s",
                      definition.definition_name.c_str());
        return result;
    }

    struct BaseMaterialInfoRegistryEntry
    {
        bool has_preset = false;
        BuiltinMaterialCreatorID preset = BuiltinMaterialCreatorID::VertexColor2D;
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

bool ShouldUse2DFallbackMaterial(const MaterialDefinitionBuildRequest &request)
{
    const GeometryVertexFormat *gvf = request.geometry_vertex_format;
    if (!gvf)
        return false;

    const GeometryVertexAttributeFormat *position = gvf->Find(VertexSemantic::Position);
    if (!position)
        return false;

    return position->vec_size == 2;
}


const char *GetBuiltinMaterialCreatorIDName(const BuiltinMaterialCreatorID mtl_id)
{
    static const char *const names[] = {
        "VertexColor2D", "PureColor2D", "PureTexture2D",
        "RectTexture2D", "RectTexture2DArray", "Text2D",
        "PureColor3D", "VertexColor3D", "VertexLuminance3D",
        "VertexPattleColor3D", "Gizmo3D", "SkyMinimal",
        "Standard", "StandardTextureArray"
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
