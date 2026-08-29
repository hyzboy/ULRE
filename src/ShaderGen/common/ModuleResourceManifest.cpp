#include <hgl/mtl/ModuleResourceManifest.h>
#include <hgl/mtl/GLSLCodeModuleRegistry.h>
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

        bool CStrEqual(const char *lhs, const char *rhs) noexcept
        {
            return lhs && rhs && std::strcmp(lhs, rhs) == 0;
        }

        uint64 GetModuleStableID(const char *name) noexcept
        {
            if (!name || !*name)
                return 0;

            hgl::hash::FNV1aHasher64 h;
            h << name;
            return h;
        }

        bool AddSSBO(ModuleResourceManifest &manifest, const GLSLCodeModuleSSBORequirement &incoming)
        {
            if (!incoming.name || !incoming.name[0])
            {
                manifest.error = ModuleResourceManifestError::ResourceConflict;
                return false;
            }

            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                auto &existing = manifest.ssbos[i];
                if (!CStrEqual(existing.name, incoming.name)
                 || existing.material_private_data_slot != incoming.material_private_data_slot)
                    continue;

                if (existing.ssbo_type != incoming.ssbo_type)
                {
                    manifest.error = ModuleResourceManifestError::ResourceConflict;
                    return false;
                }

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.ssbo_count >= MaxModuleResourceManifestSSBOs)
            {
                manifest.error = ModuleResourceManifestError::SSBOCapacityExceeded;
                return false;
            }

            manifest.ssbos[manifest.ssbo_count++] = incoming;
            return true;
        }

        bool AddTextureLayer(
            ModuleResourceManifest &manifest,
            const GLSLCodeModuleTextureLayerRequirement &incoming)
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

            if (manifest.texture_layer_count >= MaxModuleResourceManifestTextureLayers)
            {
                manifest.error = ModuleResourceManifestError::TextureLayerCapacityExceeded;
                return false;
            }

            manifest.texture_layers[manifest.texture_layer_count++] = incoming;
            return true;
        }

        const GLSLCodeModuleDefinition *FindModuleByName(
            const char *name,
            const GLSLCodeModuleRegistry *registry) noexcept
        {
            if (!registry || !name || !*name)
                return nullptr;

            return registry->FindByName(name);
        }

        bool AddModule(
            const char *name,
            const GLSLCodeModuleRegistry *registry,
            uint64 *visited_ids,
            VisitState *states,
            uint32 &visited_count,
            VisitState *mutable_states,
            ModuleResourceManifest &manifest)
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
                if (visited_count >= MaxModuleResourceManifestCodeModules)
                {
                    manifest.error = ModuleResourceManifestError::CodeModuleCapacityExceeded;
                    manifest.error_module_name = name;
                    return false;
                }
                state_index = static_cast<int>(visited_count);
                visited_ids[visited_count] = stable_id;
                states[visited_count] = VisitState::Unvisited;
                ++visited_count;
            }

            const GLSLCodeModuleDefinition *definition = FindModuleByName(name, registry);
            if (!definition)
            {
                manifest.error = ModuleResourceManifestError::UnknownCodeModule;
                manifest.error_module_name = name;
                return false;
            }

            if (!definition->glsl_code)
            {
                manifest.error = ModuleResourceManifestError::ResourceConflict;
                manifest.error_module_name = name;
                return false;
            }

            if (states[state_index] == VisitState::Visiting)
            {
                manifest.error = ModuleResourceManifestError::CodeModuleCycle;
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

            if (manifest.code_module_count >= MaxModuleResourceManifestCodeModules)
            {
                manifest.error = ModuleResourceManifestError::CodeModuleCapacityExceeded;
                manifest.error_module_name = name;
                return false;
            }

            manifest.code_module_names[manifest.code_module_count++] = definition->name;
            mutable_states[state_index] = VisitState::Visited;
            return true;
        }

        void BuildStableHash(
            ModuleResourceManifest &manifest,
            const GLSLCodeModuleRegistry *registry)
        {
            hgl::hash::FNV1aHasher64 h;
            // code modules identified by name.
            h << manifest.code_module_count;
            for (uint32 i = 0; i < manifest.code_module_count; ++i)
            {
                const char *const name = manifest.code_module_names[i];
                const auto *definition = FindModuleByName(name, registry);
                h << name
                  << (definition ? GetGLSLCodeModuleDefinitionHash(*definition) : 0);
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

    bool BuildModuleResourceManifest(
        const char *const *root_module_names,
        const uint32 root_module_count,
        ModuleResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry) noexcept
    {
        manifest = ModuleResourceManifest{};
        if (!root_module_names && root_module_count > 0)
        {
            manifest.error = ModuleResourceManifestError::NullRootList;
            return false;
        }

        uint64 visited_ids[MaxModuleResourceManifestCodeModules]{};
        VisitState states[MaxModuleResourceManifestCodeModules]{};
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

    const char *GetModuleResourceManifestErrorName(const ModuleResourceManifestError error) noexcept
    {
        switch (error)
        {
        case ModuleResourceManifestError::None: return "None";
        case ModuleResourceManifestError::NullRootList: return "NullRootList";
        case ModuleResourceManifestError::UnknownCodeModule: return "UnknownCodeModule";
        case ModuleResourceManifestError::CodeModuleCycle: return "CodeModuleCycle";
        case ModuleResourceManifestError::CodeModuleCapacityExceeded: return "CodeModuleCapacityExceeded";
        case ModuleResourceManifestError::SSBOCapacityExceeded: return "SSBOCapacityExceeded";
        case ModuleResourceManifestError::TextureLayerCapacityExceeded: return "TextureLayerCapacityExceeded";
        case ModuleResourceManifestError::ResourceConflict: return "ResourceConflict";
        }

        return "Unknown";
    }
}
