#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include "builder/DescriptorBuilderCommon.h"
#include <hgl/util/hash/FNV1a.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        enum class VisitState : uint8
        {
            Unvisited = 0,
            Visiting,
            Visited
        };

        uint64 GetModuleStableID(const char *name) noexcept
        {
            if (!name || !*name)
                return 0;

            hgl::hash::FNV1aHasher64 h;
            h << name;
            return h;
        }

        bool AddSSBO(ShaderCodeResourceManifest &manifest, const ShaderCodeModuleSSBORequirement &incoming)
        {
            if (!incoming.name || !incoming.name[0])
            {
                manifest.error = ShaderCodeResourceManifestError::ResourceConflict;
                return false;
            }

            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                auto &existing = manifest.ssbos[i];
                if (!descriptor_builder_common::CStrEqual(existing.name, incoming.name)
                 || existing.material_private_data_slot != incoming.material_private_data_slot)
                    continue;

                if (existing.ssbo_type != incoming.ssbo_type)
                {
                    manifest.error = ShaderCodeResourceManifestError::ResourceConflict;
                    return false;
                }

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.ssbo_count >= MaxShaderCodeResourceManifestSSBOs)
            {
                manifest.error = ShaderCodeResourceManifestError::SSBOCapacityExceeded;
                return false;
            }

            manifest.ssbos[manifest.ssbo_count++] = incoming;
            return true;
        }

        bool AddTextureLayer(
            ShaderCodeResourceManifest &manifest,
            const ShaderCodeModuleTextureLayerRequirement &incoming)
        {
            for (uint32 i = 0; i < manifest.texture_layer_count; ++i)
            {
                auto &existing = manifest.texture_layers[i];
                if (existing.slot != incoming.slot)
                    continue;

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.texture_layer_count >= MaxShaderCodeResourceManifestTextureLayers)
            {
                manifest.error = ShaderCodeResourceManifestError::TextureLayerCapacityExceeded;
                return false;
            }

            manifest.texture_layers[manifest.texture_layer_count++] = incoming;
            return true;
        }

        const ShaderCodeModuleDefinition *FindModuleByName(
            const char *name,
            const ShaderCodeModuleRegistry *registry) noexcept
        {
            if (!registry || !name || !*name)
                return nullptr;

            return registry->FindByName(name);
        }

        bool AddModule(
            const char *name,
            const ShaderCodeModuleRegistry *registry,
            uint64 *visited_ids,
            VisitState *states,
            uint32 &visited_count,
            VisitState *mutable_states,
            ShaderCodeResourceManifest &manifest)
        {
            const uint64 stable_id = GetModuleStableID(name);
            int state_index = -1;
            for (uint32 i = 0; i < visited_count; ++i)
            {
                if (visited_ids[i] == stable_id)
                {
                    state_index = static_cast<int>(i);
                    break;
                }
            }
            if (state_index < 0)
            {
                if (visited_count >= MaxShaderCodeResourceManifestCodeModules)
                {
                    manifest.error = ShaderCodeResourceManifestError::CodeModuleCapacityExceeded;
                    manifest.error_module_name = name;
                    return false;
                }
                state_index = static_cast<int>(visited_count);
                visited_ids[visited_count] = stable_id;
                states[visited_count] = VisitState::Unvisited;
                ++visited_count;
            }

            const ShaderCodeModuleDefinition *definition = FindModuleByName(name, registry);
            if (!definition)
            {
                manifest.error = ShaderCodeResourceManifestError::UnknownCodeModule;
                manifest.error_module_name = name;
                return false;
            }

            if (!definition->glsl_code)
            {
                manifest.error = ShaderCodeResourceManifestError::ResourceConflict;
                manifest.error_module_name = name;
                return false;
            }

            if (states[state_index] == VisitState::Visiting)
            {
                manifest.error = ShaderCodeResourceManifestError::CodeModuleCycle;
                manifest.error_module_name = name;
                return false;
            }

            if (states[state_index] == VisitState::Visited)
                return true;

            mutable_states[state_index] = VisitState::Visiting;

            for (uint32 i = 0; i < definition->dependency_count; ++i)
            {
                if (!AddModule(definition->dependencies[i].module_name, registry,
                               visited_ids, states, visited_count, mutable_states, manifest))
                    return false;
            }

            for (uint32 i = 0; i < definition->ssbo_requirement_count; ++i)
            {
                if (!AddSSBO(manifest, definition->ssbo_requirements[i]))
                    return false;
            }

            for (uint32 i = 0; i < definition->texture_layer_requirement_count; ++i)
            {
                if (!AddTextureLayer(manifest, definition->texture_layer_requirements[i]))
                    return false;
            }

            if (manifest.code_module_count >= MaxShaderCodeResourceManifestCodeModules)
            {
                manifest.error = ShaderCodeResourceManifestError::CodeModuleCapacityExceeded;
                manifest.error_module_name = name;
                return false;
            }

            manifest.code_module_names[manifest.code_module_count++] = definition->name;
            mutable_states[state_index] = VisitState::Visited;
            return true;
        }

        void BuildStableHash(
            ShaderCodeResourceManifest &manifest,
            const ShaderCodeModuleRegistry *registry)
        {
            hgl::hash::FNV1aHasher64 h;
            // code modules identified by name.
            h << manifest.code_module_count;
            for (uint32 i = 0; i < manifest.code_module_count; ++i)
            {
                const char *const name = manifest.code_module_names[i];
                const auto *definition = FindModuleByName(name, registry);
                h << name
                  << (definition ? GetShaderCodeModuleDefinitionHash(*definition) : 0);
            }

            h << manifest.ssbo_count;
            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                const auto &ssbo = manifest.ssbos[i];
                h << ssbo.name;
                h << ssbo.ssbo_type
                  << ssbo.material_private_data_slot
                  << ssbo.stage_flags
                  << ssbo.required
                  << ssbo.allow_fallback;
            }

            h << manifest.texture_layer_count;
            for (uint32 i = 0; i < manifest.texture_layer_count; ++i)
                h << manifest.texture_layers[i];

            manifest.stable_hash = h;
        }
    }

    bool BuildShaderCodeResourceManifest(
        const char *const *root_module_names,
        const uint32 root_module_count,
        ShaderCodeResourceManifest &manifest,
        const ShaderCodeModuleRegistry *registry) noexcept
    {
        manifest = ShaderCodeResourceManifest{};
        if (!root_module_names && root_module_count > 0)
        {
            manifest.error = ShaderCodeResourceManifestError::NullRootList;
            return false;
        }

        uint64 visited_ids[MaxShaderCodeResourceManifestCodeModules]{};
        VisitState states[MaxShaderCodeResourceManifestCodeModules]{};
        uint32 visited_count = 0;

        for (uint32 i = 0; i < root_module_count; ++i)
        {
            if (!AddModule(root_module_names[i], registry, visited_ids, states,
                           visited_count, states, manifest))
                return false;
        }

        BuildStableHash(manifest, registry);
        return true;
    }

    const char *GetShaderCodeResourceManifestErrorName(const ShaderCodeResourceManifestError error) noexcept
    {
#define HGL_ERROR(name) case ShaderCodeResourceManifestError::name: return #name;
        switch (error)
        {
            HGL_MODULE_RESOURCE_MANIFEST_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }
}
