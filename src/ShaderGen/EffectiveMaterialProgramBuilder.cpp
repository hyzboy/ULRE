#include <hgl/shadergen/EffectiveMaterialProgramBuilder.h>

#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/shadergen/MaterialOutputContract.h>
#include <hgl/shadergen/ShaderProgramLinkSpec.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    namespace
    {
        SurfaceProfileID ResolveSurfaceProfileID(
            const MaterialDefinition &definition) noexcept
        {
            if (!definition.surface_profile_projections.IsEmpty())
                return definition.surface_profile_projections[0].profile_id;

            return GetSurfaceStableID(
                GetSurfaceTypeName(definition.compositor_surface));
        }

        SurfaceProjectionID ResolveSurfaceProjectionID(
            const MaterialDefinition &definition) noexcept
        {
            if (!definition.surface_profile_projections.IsEmpty())
            {
                return definition.surface_profile_projections[0].
                    projection_id;
            }
            if (definition.fragment_material_source_module)
            {
                return GetSurfaceStableID(
                    definition.fragment_material_source_module);
            }
            if (definition.fragment_surface_module)
                return GetSurfaceStableID(
                    definition.fragment_surface_module);
            return GetSurfaceStableID(definition.fragment_source);
        }

        ShaderProgramPurpose ResolvePurpose(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            return request.override_shader_program_purpose
                ? request.shader_program_purpose
                : GetShaderProgramPurpose(definition.compositor_pass);
        }
    }

    bool BuildEffectiveMaterialProgram(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ShaderProgramLinkSpec &program_link,
        EffectiveMaterialProgramKey &out_effective_program,
        MaterialResolutionResult &out_resolution) noexcept
    {
        out_effective_program = {};
        out_resolution = {};
        if (!program_link.IsValid())
            return false;

        out_effective_program.resolved_surface_profile_id =
            ResolveSurfaceProfileID(definition);
        out_effective_program.resolved_surface_profile_hash =
            hgl::hash::FNV1aAppendValueBytes(
                hgl::hash::FNV1aInit<uint64>(),
                out_effective_program.resolved_surface_profile_id);
        out_effective_program.projection_id =
            ResolveSurfaceProjectionID(definition);

        uint64 static_feature_hash = hgl::hash::FNV1aInit<uint64>();
        static_feature_hash = hgl::hash::FNV1aAppendValueBytes(
            static_feature_hash,
            program_link.vertex_stage.definition_hash);
        static_feature_hash = hgl::hash::FNV1aAppendValueBytes(
            static_feature_hash,
            program_link.fragment_stage.definition_hash);
        out_effective_program.normalized_static_feature_hash =
            hgl::hash::FNV1aAppendValueBytes(
                static_feature_hash,
                program_link.pipeline_state_hash);

        uint64 module_graph_hash = hgl::hash::FNV1aInit<uint64>();
        module_graph_hash = hgl::hash::FNV1aAppendValueBytes(
            module_graph_hash,
            program_link.vertex_stage.glsl_module_graph_hash);
        out_effective_program.resolved_module_graph_hash =
            hgl::hash::FNV1aAppendValueBytes(
                module_graph_hash,
                program_link.fragment_stage.glsl_module_graph_hash);

        uint64 capability_hash = hgl::hash::FNV1aInit<uint64>();
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.vertex_stage.interface_hash);
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.fragment_stage.interface_hash);
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.vertex_stage.resource_hash);
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.fragment_stage.resource_hash);
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.resource_layout_hash);
        capability_hash = hgl::hash::FNV1aAppendValueBytes(
            capability_hash,
            program_link.vertex_input_hash);
        out_effective_program.capability_signature_hash =
            hgl::hash::FNV1aAppendValueBytes(
                capability_hash,
                program_link.render_target_hash);

        uint64 resolver_policy_hash = hgl::hash::FNV1aInit<uint64>();
        resolver_policy_hash = hgl::hash::FNV1aAppendValueBytes(
            resolver_policy_hash,
            MaterialProgramContractSchemaVersion);
        resolver_policy_hash = hgl::hash::FNV1aAppendValueBytes(
            resolver_policy_hash,
            SurfaceProfileSchemaVersion);
        out_effective_program.resolver_policy_hash =
            hgl::hash::FNV1aAppendValueBytes(
                resolver_policy_hash,
                program_link.compiler_hash);
        out_effective_program.purpose =
            ResolvePurpose(definition, request);
        if (!ValidateEffectiveMaterialProgramKey(
                out_effective_program))
            return false;

        out_resolution.status = MaterialResolutionStatus::Resolved;
        out_resolution.source_definition_id_hash =
            GetSurfaceStableID(definition.definition_id.c_str());
        out_resolution.recipe_capability_hash =
            HashMaterialRecipe(request.recipe);
        out_resolution.source_surface_intent_id =
            definition.surface_intent_id != InvalidSurfaceIntentID
                ? definition.surface_intent_id
                : GetSurfaceStableID(
                    GetSurfaceTypeName(
                        definition.compositor_surface));
        out_resolution.requested_quality_class = 0;
        out_resolution.resolved_material_lod =
            request.recipe.material_lod;
        out_resolution.fallback_depth = 0;
        out_resolution.effective_program = out_effective_program;
        return ValidateMaterialResolutionResult(out_resolution);
    }
}
