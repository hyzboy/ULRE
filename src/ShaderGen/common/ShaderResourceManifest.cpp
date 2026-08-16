#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
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

        bool AddUBO(ShaderResourceManifest &manifest, const GLSLCodeModuleUBORequirement &incoming)
        {
            for (uint32 i = 0; i < manifest.ubo_count; ++i)
            {
                auto &existing = manifest.ubos[i];
                if (existing.semantic != incoming.semantic)
                    continue;

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.ubo_count >= MaxShaderResourceManifestUBOs)
            {
                manifest.error = ShaderResourceManifestError::UBOCapacityExceeded;
                return false;
            }

            manifest.ubos[manifest.ubo_count++] = incoming;
            return true;
        }

        bool AddSSBO(ShaderResourceManifest &manifest, const GLSLCodeModuleSSBORequirement &incoming)
        {
            if (!incoming.name || !incoming.name[0])
            {
                manifest.error = ShaderResourceManifestError::ResourceConflict;
                return false;
            }

            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                auto &existing = manifest.ssbos[i];
                if (!CStrEqual(existing.name, incoming.name)
                 || existing.data_slot != incoming.data_slot)
                    continue;

                if (existing.ssbo_type != incoming.ssbo_type)
                {
                    manifest.error = ShaderResourceManifestError::ResourceConflict;
                    return false;
                }

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.ssbo_count >= MaxShaderResourceManifestSSBOs)
            {
                manifest.error = ShaderResourceManifestError::SSBOCapacityExceeded;
                return false;
            }

            manifest.ssbos[manifest.ssbo_count++] = incoming;
            return true;
        }

        bool AddTextureLayer(
            ShaderResourceManifest &manifest,
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

            if (manifest.texture_layer_count >= MaxShaderResourceManifestTextureLayers)
            {
                manifest.error = ShaderResourceManifestError::TextureLayerCapacityExceeded;
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
            ShaderResourceManifest &manifest)
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
                if (visited_count >= MaxShaderResourceManifestCodeModules)
                {
                    manifest.error = ShaderResourceManifestError::CodeModuleCapacityExceeded;
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
                manifest.error = ShaderResourceManifestError::UnknownCodeModule;
                manifest.error_module_name = name;
                return false;
            }

            if (!definition->glsl_code)
            {
                manifest.error = ShaderResourceManifestError::ResourceConflict;
                manifest.error_module_name = name;
                return false;
            }

            if (states[state_index] == VisitState::Visiting)
            {
                manifest.error = ShaderResourceManifestError::CodeModuleCycle;
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

            for (uint32 i = 0; i < definition->ubo_requirement_count; ++i)
            {
                if (!AddUBO(manifest, definition->ubo_requirements[i]))
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

            if (manifest.code_module_count >= MaxShaderResourceManifestCodeModules)
            {
                manifest.error = ShaderResourceManifestError::CodeModuleCapacityExceeded;
                manifest.error_module_name = name;
                return false;
            }

            manifest.code_module_names[manifest.code_module_count++] = definition->name;
            mutable_states[state_index] = VisitState::Visited;
            return true;
        }

        void BuildStableHash(
            ShaderResourceManifest &manifest,
            const GLSLCodeModuleRegistry *registry)
        {
            hgl::hash::FNV1aHasher64 h;
            // v3: code modules identified by name (numeric ID track removed).
            constexpr uint32 manifest_version = 3u;
            h << manifest_version;

            h << manifest.code_module_count;
            for (uint32 i = 0; i < manifest.code_module_count; ++i)
            {
                const char *const name = manifest.code_module_names[i];
                const auto *definition = FindModuleByName(name, registry);
                h << name
                  << (definition ? GetGLSLCodeModuleDefinitionHash(*definition) : 0);
            }

            h << manifest.ubo_count;
            for (uint32 i = 0; i < manifest.ubo_count; ++i)
                h << manifest.ubos[i];

            h << manifest.ssbo_count;
            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                const auto &ssbo = manifest.ssbos[i];
                h << ssbo.name;
                h << ssbo.ssbo_type
                  << ssbo.data_slot
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

    bool BuildShaderResourceManifest(
        const char *const *root_module_names,
        const uint32 root_module_count,
        ShaderResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry) noexcept
    {
        manifest = ShaderResourceManifest{};
        if (!root_module_names && root_module_count > 0)
        {
            manifest.error = ShaderResourceManifestError::NullRootList;
            return false;
        }

        uint64 visited_ids[MaxShaderResourceManifestCodeModules]{};
        VisitState states[MaxShaderResourceManifestCodeModules]{};
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

    const char *GetShaderResourceManifestErrorName(const ShaderResourceManifestError error) noexcept
    {
        switch (error)
        {
        case ShaderResourceManifestError::None: return "None";
        case ShaderResourceManifestError::NullRootList: return "NullRootList";
        case ShaderResourceManifestError::UnknownCodeModule: return "UnknownCodeModule";
        case ShaderResourceManifestError::CodeModuleCycle: return "CodeModuleCycle";
        case ShaderResourceManifestError::CodeModuleCapacityExceeded: return "CodeModuleCapacityExceeded";
        case ShaderResourceManifestError::UBOCapacityExceeded: return "UBOCapacityExceeded";
        case ShaderResourceManifestError::SSBOCapacityExceeded: return "SSBOCapacityExceeded";
        case ShaderResourceManifestError::TextureLayerCapacityExceeded: return "TextureLayerCapacityExceeded";
        case ShaderResourceManifestError::ResourceConflict: return "ResourceConflict";
        }

        return "Unknown";
    }
}
