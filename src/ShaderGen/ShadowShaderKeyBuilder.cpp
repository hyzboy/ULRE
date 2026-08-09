#include <hgl/shadergen/ShadowShaderKeyBuilder.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/SurfaceProfile.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderKeyUtility.h>
#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/util/hash/FNV1a.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetKeyFailure(
            ShadowShaderKeyBuildDiagnostic &diagnostic,
            const ShadowShaderKeyBuildError error,
            const char *detail)
        {
            diagnostic.error = error;
            diagnostic.detail = detail ? detail : "";
            return false;
        }

        uint64 AppendText(
            uint64 hash,
            const char *text,
            const size_t length) noexcept
        {
            const uint32 size = static_cast<uint32>(length);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, size);
            return size > 0
                ? hgl::hash::FNV1aAppendBytes(hash, text, size)
                : hash;
        }

        uint64 AppendText(uint64 hash, const char *text) noexcept
        {
            return AppendText(
                hash, text ? text : "", text ? std::strlen(text) : 0);
        }

        uint64 AppendText(
            uint64 hash,
            const std::string &text) noexcept
        {
            return AppendText(hash, text.data(), text.size());
        }

        uint64 HashText(const std::string &text) noexcept
        {
            return AppendText(hgl::hash::FNV1aInit<uint64>(), text);
        }

        uint64 HashResolverPolicy(
            const MaterialDefinition &definition) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, CanonicalShaderContractSchemaVersion);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, MaterialProgramContractSchemaVersion);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, GLSLCodeModuleCurrentMetadataVersion);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, SurfaceProfileSchemaVersion);
            return hgl::hash::FNV1aAppendValueBytes(
                hash, definition.vertex_provider_policy);
        }

        uint64 HashStaticFeatures(
            const MaterialDefinition &definition,
            const MaterialRecipe &recipe) noexcept
        {
            const ResolvedMaterialRenderState state =
                ResolveMaterialRenderState(definition, recipe);
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.fragment_program_mode);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_surface);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_blend);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_pass);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, state.alpha_test);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, state.alpha_cutoff);
            return hgl::hash::FNV1aAppendValueBytes(hash, state.dither);
        }

        uint64 HashRecipeCapabilities(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            struct TextureCapability
            {
                TextureSlot slot = TextureSlot::BaseColor;
                bool available = false;
                bool direct = false;
                bool required = false;
            };
            struct SSBOCapability
            {
                uint32 data_slot = 0;
                SSBOType type = SSBOType::UserDefined;
                bool available = false;
                bool uses_data_index = false;
                bool shared = false;
            };

            std::vector<TextureCapability> textures;
            textures.reserve(request.recipe.textures.size());
            for (const RecipeTextureBinding &texture :
                 request.recipe.textures)
            {
                textures.push_back(
                    {
                        texture.slot,
                        texture.use_direct_value
                            || !texture.resource_id.empty(),
                        texture.use_direct_value,
                        texture.required
                    });
            }
            std::sort(
                textures.begin(),
                textures.end(),
                [](const TextureCapability &lhs,
                   const TextureCapability &rhs)
                {
                    if (lhs.slot != rhs.slot)
                        return lhs.slot < rhs.slot;
                    if (lhs.available != rhs.available)
                        return lhs.available < rhs.available;
                    if (lhs.direct != rhs.direct)
                        return lhs.direct < rhs.direct;
                    return lhs.required < rhs.required;
                });

            std::vector<SSBOCapability> ssbos;
            ssbos.reserve(request.recipe.ssbo_assets.size());
            for (const RecipeSSBOAssetBinding &ssbo :
                 request.recipe.ssbo_assets)
            {
                ssbos.push_back(
                    {
                        ssbo.data_slot,
                        ssbo.ssbo_type,
                        ssbo.ssbo_id != 0,
                        ssbo.use_data_index,
                        ssbo.shared_across_instances
                    });
            }
            std::sort(
                ssbos.begin(),
                ssbos.end(),
                [](const SSBOCapability &lhs,
                   const SSBOCapability &rhs)
                {
                    if (lhs.data_slot != rhs.data_slot)
                        return lhs.data_slot < rhs.data_slot;
                    if (lhs.type != rhs.type)
                        return lhs.type < rhs.type;
                    if (lhs.available != rhs.available)
                        return lhs.available < rhs.available;
                    if (lhs.uses_data_index != rhs.uses_data_index)
                        return lhs.uses_data_index < rhs.uses_data_index;
                    return lhs.shared < rhs.shared;
                });

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = AppendText(hash, request.recipe.domain);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.recipe.material_lod);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, HashStaticFeatures(definition, request.recipe));
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(textures.size()));
            for (const TextureCapability &texture : textures)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.slot);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, texture.available);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.direct);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, texture.required);
            }
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(ssbos.size()));
            for (const SSBOCapability &ssbo : ssbos)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, ssbo.data_slot);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.type);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, ssbo.available);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, ssbo.uses_data_index);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.shared);
            }
            return hash;
        }

        uint64 HashDefinitionSelectionContent(
            const MaterialDefinition &definition,
            const uint64 graph_hash,
            const uint64 interface_hash,
            const uint64 output_hash) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, graph_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, interface_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, output_hash);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.source_kind);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.usage_tag);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.bootstrap_kind);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.default_lod);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.lod_count);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.surface_intent_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.fragment_program_mode);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_surface);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_blend);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.compositor_pass);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.vertex_provider_policy);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash,
                HashResolvedMaterialRenderState(
                    definition.default_render_state));

            std::vector<MaterialSurfaceProfileProjection> projections =
                definition.surface_profile_projections.GetArray();
            std::sort(
                projections.begin(),
                projections.end(),
                [](const MaterialSurfaceProfileProjection &lhs,
                   const MaterialSurfaceProfileProjection &rhs)
                {
                    if (lhs.profile_id != rhs.profile_id)
                        return lhs.profile_id < rhs.profile_id;
                    if (lhs.projection_id != rhs.projection_id)
                        return lhs.projection_id < rhs.projection_id;
                    return lhs.projection_schema_version
                        < rhs.projection_schema_version;
                });
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(projections.size()));
            for (const auto &projection : projections)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, projection.profile_id);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, projection.projection_id);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, projection.projection_schema_version);
            }

            std::vector<MaterialDataSlotDecl> data_slots =
                definition.data_slot_decls;
            std::sort(
                data_slots.begin(),
                data_slots.end(),
                [](const MaterialDataSlotDecl &lhs,
                   const MaterialDataSlotDecl &rhs)
                {
                    if (lhs.name != rhs.name)
                        return lhs.name < rhs.name;
                    return lhs.ssbo_type < rhs.ssbo_type;
                });
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(data_slots.size()));
            for (const MaterialDataSlotDecl &slot : data_slots)
            {
                hash = AppendText(hash, slot.name);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, slot.ssbo_type);
            }

            std::vector<UBODescriptorSemantic> ubos =
                definition.ubo_requirements;
            std::sort(ubos.begin(), ubos.end());
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(ubos.size()));
            for (const UBODescriptorSemantic ubo : ubos)
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ubo);

            std::vector<MaterialTextureSlotDecl> textures =
                definition.texture_slot_decls;
            std::sort(
                textures.begin(),
                textures.end(),
                [](const MaterialTextureSlotDecl &lhs,
                   const MaterialTextureSlotDecl &rhs)
                {
                    if (lhs.slot != rhs.slot)
                        return lhs.slot < rhs.slot;
                    if (lhs.sampler_type != rhs.sampler_type)
                        return lhs.sampler_type < rhs.sampler_type;
                    if (lhs.required != rhs.required)
                        return lhs.required < rhs.required;
                    const char *left_name = lhs.name ? lhs.name : "";
                    const char *right_name = rhs.name ? rhs.name : "";
                    return std::strcmp(left_name, right_name) < 0;
                });
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(textures.size()));
            for (const MaterialTextureSlotDecl &texture : textures)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, texture.slot);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, texture.sampler_type);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, texture.required);
                hash = AppendText(hash, texture.name);
            }

            std::vector<GLSLCodeModuleSemanticRequirement> vertex_requirements =
                definition.vertex_semantic_requirements.GetArray();
            std::sort(
                vertex_requirements.begin(),
                vertex_requirements.end(),
                [](const GLSLCodeModuleSemanticRequirement &lhs,
                   const GLSLCodeModuleSemanticRequirement &rhs)
                {
                    if (lhs.source != rhs.source)
                        return lhs.source < rhs.source;
                    if (lhs.semantic != rhs.semantic)
                        return lhs.semantic < rhs.semantic;
                    if (lhs.numeric_class_mask != rhs.numeric_class_mask)
                        return lhs.numeric_class_mask
                            < rhs.numeric_class_mask;
                    if (lhs.min_component_count != rhs.min_component_count)
                        return lhs.min_component_count
                            < rhs.min_component_count;
                    return lhs.max_component_count
                        < rhs.max_component_count;
                });
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash,
                static_cast<uint32>(vertex_requirements.size()));
            for (const auto &requirement : vertex_requirements)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.source);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.semantic);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.numeric_class_mask);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.min_component_count);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, requirement.max_component_count);
            }
            return hash;
        }

        uint64 HashRequestContext(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            const VertexShaderNodeConfig node_config =
                ResolveMaterialVertexNodeConfig(definition, request);
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, node_config);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.primitive_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.override_shader_stage_bits);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.shader_stage_flag_bit);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.override_sky_ambient_model);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, request.sky_ambient_model);
            return hash;
        }

        uint64 HashGeometryCapabilities(
            const MaterialDefinitionBuildRequest &request,
            const ShaderInterfaceContract &shader_interface) noexcept
        {
            if (request.geometry_vertex_format)
                return request.geometry_vertex_format->GetVertexInputHash();

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            for (int i = 0;
                 i < shader_interface.geometry_semantics.GetCount();
                 ++i)
            {
                const GeometrySemanticContractEntry &entry =
                    shader_interface.geometry_semantics[i];
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, entry.semantic);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, entry.scalar_type);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, entry.component_count);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, entry.physical_format);
            }
            return hash;
        }

        SurfaceProfileID ResolveProfileID(
            const MaterialDefinition &definition) noexcept
        {
            return definition.surface_profile_projections.IsEmpty()
                ? GetSurfaceStableID(
                    GetSurfaceTypeName(definition.compositor_surface))
                : definition.surface_profile_projections[0].profile_id;
        }

        SurfaceProjectionID ResolveProjectionID(
            const MaterialDefinition &definition) noexcept
        {
            if (!definition.surface_profile_projections.IsEmpty())
                return definition.surface_profile_projections[0].
                    projection_id;
            if (definition.fragment_material_source_module)
                return GetSurfaceStableID(
                    definition.fragment_material_source_module);
            if (definition.fragment_surface_module)
                return GetSurfaceStableID(
                    definition.fragment_surface_module);
            return GetSurfaceStableID(definition.fragment_source);
        }

        uint64 HashCapabilitySignature(
            const uint64 interface_hash,
            const uint64 output_hash) noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, interface_hash);
            return hgl::hash::FNV1aAppendValueBytes(hash, output_hash);
        }
    }

    const char *GetShadowShaderKeyBuildErrorName(
        const ShadowShaderKeyBuildError error) noexcept
    {
        switch (error)
        {
        case ShadowShaderKeyBuildError::None: return "None";
        case ShadowShaderKeyBuildError::InvalidModuleGraph: return "InvalidModuleGraph";
        case ShadowShaderKeyBuildError::InvalidShaderContract: return "InvalidShaderContract";
        case ShadowShaderKeyBuildError::MissingLegacyStage: return "MissingLegacyStage";
        case ShadowShaderKeyBuildError::InvalidSelectionKey: return "InvalidSelectionKey";
        case ShadowShaderKeyBuildError::InvalidEffectiveProgramKey: return "InvalidEffectiveProgramKey";
        case ShadowShaderKeyBuildError::InvalidShaderVariant: return "InvalidShaderVariant";
        case ShadowShaderKeyBuildError::InvalidStageKey: return "InvalidStageKey";
        case ShadowShaderKeyBuildError::InvalidProgramKey: return "InvalidProgramKey";
        case ShadowShaderKeyBuildError::InvalidProgramMetadata: return "InvalidProgramMetadata";
        }
        return "Unknown";
    }

    bool ValidateShaderProgramArtifactMetadata(
        const ShaderProgramArtifactMetadata &metadata) noexcept
    {
        return metadata.schema_version
                == ShaderProgramMetadataSchemaVersion
            && metadata.program_key_digest != 0
            && metadata.effective_material_program_digest != 0
            && metadata.shader_variant_digest != 0
            && metadata.resolved_module_graph_hash != 0
            && metadata.shader_interface_hash != 0
            && metadata.output_contract_hash != 0
            && metadata.vertex_stage_digest != 0
            && metadata.fragment_stage_digest != 0
            && metadata.compiler_profile_hash != 0
            && metadata.device_target_hash != 0
            && metadata.generated_source_digest != 0;
    }

    uint64 GetShaderProgramArtifactMetadataHash(
        const ShaderProgramArtifactMetadata &metadata) noexcept
    {
        if (!ValidateShaderProgramArtifactMetadata(metadata))
            return 0;

        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.schema_version);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.program_key_digest);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.effective_material_program_digest);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.shader_variant_digest);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.resolved_module_graph_hash);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.shader_interface_hash);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.output_contract_hash);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.vertex_stage_digest);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.fragment_stage_digest);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.compiler_profile_hash);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.device_target_hash);
        return hgl::hash::FNV1aAppendValueBytes(
            hash, metadata.generated_source_digest);
    }

    bool BuildShadowShaderKeys(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ResolvedModuleGraph &module_graph,
        const ShadowShaderContracts &contracts,
        const ShaderProgramBuildSpec &legacy_build_spec,
        ShadowShaderKeys &out_keys,
        ShadowShaderKeyBuildDiagnostic &out_diagnostic)
    {
        out_keys = {};
        out_diagnostic = {};

        const uint64 graph_hash = GetResolvedModuleGraphHash(module_graph);
        if (graph_hash == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidModuleGraph,
                "resolved module graph hash is invalid");

        const uint64 interface_hash =
            GetShaderInterfaceContractHash(contracts.shader_interface);
        const uint64 output_hash =
            GetOutputContractHash(contracts.output);
        if (interface_hash == 0 || output_hash == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidShaderContract,
                "shadow interface or output hash is invalid");

        const ShaderCreateInfo *vertex =
            legacy_build_spec.GetStageShader(ShaderStage::Vertex);
        const ShaderCreateInfo *fragment =
            legacy_build_spec.GetStageShader(ShaderStage::Fragment);
        if (!vertex || !fragment
         || vertex->GetFinalGLSL().empty()
         || fragment->GetFinalGLSL().empty())
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::MissingLegacyStage,
                "generated legacy GLSL is unavailable");

        const uint64 device_hash =
            contract::GetPhysicalDeviceProfileHash(profile);
        const uint64 compiler_hash =
            contract::GetShaderCompilerProfileHash(profile);
        const uint64 static_feature_hash =
            HashStaticFeatures(definition, request.recipe);
        const uint64 capability_hash =
            HashCapabilitySignature(interface_hash, output_hash);
        const uint64 resolver_policy_hash =
            HashResolverPolicy(definition);

        out_keys.selection_request.definition_id_hash =
            GetSurfaceStableID(definition.definition_id.c_str());
        out_keys.selection_request.definition_content_hash =
            HashDefinitionSelectionContent(
                definition, graph_hash, interface_hash, output_hash);
        out_keys.selection_request.recipe_capability_hash =
            HashRecipeCapabilities(definition, request);
        out_keys.selection_request.geometry_capability_hash =
            HashGeometryCapabilities(request, contracts.shader_interface);
        out_keys.selection_request.device_quality_hash = device_hash;
        out_keys.selection_request.request_context_hash =
            HashRequestContext(definition, request);
        out_keys.selection_request.purpose = contracts.output.purpose;
        out_keys.selection_request.requested_material_lod =
            request.recipe.material_lod;
        if (!ValidateMaterialSelectionRequestKey(
                out_keys.selection_request))
        {
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidSelectionKey,
                "material selection request key is invalid");
        }

        out_keys.effective_program.resolved_surface_profile_id =
            ResolveProfileID(definition);
        out_keys.effective_program.resolved_surface_profile_hash =
            hgl::hash::FNV1aAppendValueBytes(
                hgl::hash::FNV1aInit<uint64>(),
                out_keys.effective_program.resolved_surface_profile_id);
        out_keys.effective_program.projection_id =
            ResolveProjectionID(definition);
        out_keys.effective_program.normalized_static_feature_hash =
            static_feature_hash;
        out_keys.effective_program.resolved_module_graph_hash =
            graph_hash;
        out_keys.effective_program.capability_signature_hash =
            capability_hash;
        out_keys.effective_program.resolver_policy_hash =
            resolver_policy_hash;
        out_keys.effective_program.purpose = contracts.output.purpose;
        const uint64 effective_digest =
            out_keys.effective_program.GetDigest();
        if (effective_digest == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidEffectiveProgramKey,
                "effective material program key is invalid");

        out_keys.shader_variant.effective_material_program_digest =
            effective_digest;
        out_keys.shader_variant.resolved_module_graph_hash = graph_hash;
        out_keys.shader_variant.shader_interface_hash = interface_hash;
        out_keys.shader_variant.output_contract_hash = output_hash;
        out_keys.shader_variant.compile_time_feature_hash =
            static_feature_hash;
        out_keys.shader_variant.compiler_profile_hash = compiler_hash;
        out_keys.shader_variant.device_target_hash = device_hash;
        const uint64 variant_digest =
            GetShaderVariantContractHash(out_keys.shader_variant);
        if (variant_digest == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidShaderVariant,
                "shader variant key is invalid");

        const uint64 vertex_source_hash = HashFinalShaderSource(
            vertex->GetFinalGLSL().data(),
            vertex->GetFinalGLSL().size());
        const uint64 fragment_source_hash = HashFinalShaderSource(
            fragment->GetFinalGLSL().data(),
            fragment->GetFinalGLSL().size());
        out_keys.vertex_stage = BuildFinalShaderStageKey(
            ShaderStage::Vertex,
            vertex->GetFinalGLSL().data(),
            vertex->GetFinalGLSL().size(),
            graph_hash,
            interface_hash,
            interface_hash,
            compiler_hash);
        out_keys.fragment_stage = BuildFinalShaderStageKey(
            ShaderStage::Fragment,
            fragment->GetFinalGLSL().data(),
            fragment->GetFinalGLSL().size(),
            graph_hash,
            interface_hash,
            interface_hash,
            compiler_hash);
        if (out_keys.vertex_stage.GetDigest() == 0
         || out_keys.fragment_stage.GetDigest() == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidStageKey,
                "shadow stage key is invalid");

        out_keys.program.vertex_stage_digest =
            out_keys.vertex_stage.GetDigest();
        out_keys.program.fragment_stage_digest =
            out_keys.fragment_stage.GetDigest();
        out_keys.program.resource_layout_hash = interface_hash;
        out_keys.program.vertex_input_hash =
            out_keys.selection_request.geometry_capability_hash;
        out_keys.program.pipeline_state_hash = 0;
        out_keys.program.render_target_hash = output_hash;
        out_keys.program.compiler_hash = compiler_hash;
        const uint64 program_digest = out_keys.program.GetDigest();
        if (program_digest == 0)
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidProgramKey,
                "shadow program key is invalid");

        uint64 source_digest = hgl::hash::FNV1aInit<uint64>();
        source_digest = hgl::hash::FNV1aAppendValueBytes(
            source_digest, vertex_source_hash);
        source_digest = hgl::hash::FNV1aAppendValueBytes(
            source_digest, fragment_source_hash);

        out_keys.program_metadata.program_key_digest = program_digest;
        out_keys.program_metadata.effective_material_program_digest =
            effective_digest;
        out_keys.program_metadata.shader_variant_digest = variant_digest;
        out_keys.program_metadata.resolved_module_graph_hash = graph_hash;
        out_keys.program_metadata.shader_interface_hash = interface_hash;
        out_keys.program_metadata.output_contract_hash = output_hash;
        out_keys.program_metadata.vertex_stage_digest =
            out_keys.vertex_stage.GetDigest();
        out_keys.program_metadata.fragment_stage_digest =
            out_keys.fragment_stage.GetDigest();
        out_keys.program_metadata.compiler_profile_hash = compiler_hash;
        out_keys.program_metadata.device_target_hash = device_hash;
        out_keys.program_metadata.generated_source_digest = source_digest;
        if (!ValidateShaderProgramArtifactMetadata(
                out_keys.program_metadata))
        {
            return SetKeyFailure(
                out_diagnostic,
                ShadowShaderKeyBuildError::InvalidProgramMetadata,
                "shadow program metadata is invalid");
        }

        return true;
    }
}
