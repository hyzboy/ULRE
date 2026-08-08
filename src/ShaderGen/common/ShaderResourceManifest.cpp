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

        bool AddTexture(ShaderResourceManifest &manifest, const GLSLCodeModuleTextureRequirement &incoming)
        {
            if (!incoming.name || !incoming.name[0]
             || !incoming.glsl_type || !incoming.glsl_type[0])
            {
                manifest.error = ShaderResourceManifestError::ResourceConflict;
                return false;
            }

            for (uint32 i = 0; i < manifest.texture_count; ++i)
            {
                auto &existing = manifest.textures[i];
                const bool same_semantic_slot =
                    existing.semantic == incoming.semantic
                 && existing.slot == incoming.slot;
                const bool same_name = CStrEqual(existing.name, incoming.name);
                if (!same_semantic_slot && !same_name)
                    continue;

                if (!same_semantic_slot
                 || !same_name
                 || !CStrEqual(existing.glsl_type, incoming.glsl_type))
                {
                    manifest.error = ShaderResourceManifestError::ResourceConflict;
                    return false;
                }

                existing.stage_flags |= incoming.stage_flags;
                existing.required = existing.required || incoming.required;
                existing.allow_fallback = existing.allow_fallback && incoming.allow_fallback;
                return true;
            }

            if (manifest.texture_count >= MaxShaderResourceManifestTextures)
            {
                manifest.error = ShaderResourceManifestError::TextureCapacityExceeded;
                return false;
            }

            manifest.textures[manifest.texture_count++] = incoming;
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

        const GLSLCodeModuleDefinition *FindModule(
            const GLSLCodeModuleID id,
            const GLSLCodeModuleRegistry *registry) noexcept
        {
            if (registry)
            {
                const auto *definition = registry->Find(id);
                if (definition)
                    return definition;
            }
            return FindGLSLCodeModuleDefinition(id);
        }

        bool AddModule(
            const GLSLCodeModuleID id,
            const GLSLCodeModuleRegistry *registry,
            GLSLCodeModuleID *visited_ids,
            VisitState *states,
            uint32 &visited_count,
            VisitState *mutable_states,
            ShaderResourceManifest &manifest)
        {
            int state_index = -1;
            for (uint32 i = 0; i < visited_count; ++i)
            {
                if (visited_ids[i] == id)
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
                    manifest.error_module = id;
                    return false;
                }
                state_index = static_cast<int>(visited_count);
                visited_ids[visited_count] = id;
                states[visited_count] = VisitState::Unvisited;
                ++visited_count;
            }

            const GLSLCodeModuleDefinition *definition = FindModule(id, registry);
            if (!definition)
            {
                manifest.error = ShaderResourceManifestError::UnknownCodeModule;
                manifest.error_module = id;
                return false;
            }

            if (!definition->glsl_code)
            {
                manifest.error = ShaderResourceManifestError::ResourceConflict;
                manifest.error_module = id;
                return false;
            }

            if (states[state_index] == VisitState::Visiting)
            {
                manifest.error = ShaderResourceManifestError::CodeModuleCycle;
                manifest.error_module = id;
                return false;
            }

            if (states[state_index] == VisitState::Visited)
                return true;

            mutable_states[state_index] = VisitState::Visiting;

            for (uint32 i = 0; i < definition->code_module_requirement_count; ++i)
            {
                if (!AddModule(definition->code_module_requirements[i], registry,
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

            for (uint32 i = 0; i < definition->texture_requirement_count; ++i)
            {
                if (!AddTexture(manifest, definition->texture_requirements[i]))
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
                manifest.error_module = id;
                return false;
            }

            manifest.code_modules[manifest.code_module_count++] = id;
            mutable_states[state_index] = VisitState::Visited;
            return true;
        }

        void BuildStableHash(
            ShaderResourceManifest &manifest,
            const GLSLCodeModuleRegistry *registry)
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            constexpr uint32 manifest_version = 2u;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest_version);

            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.code_module_count);
            for (uint32 i = 0; i < manifest.code_module_count; ++i)
            {
                const auto id = manifest.code_modules[i];
                const auto *definition = FindModule(id, registry);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, id);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, definition ? GetGLSLCodeModuleDefinitionHash(*definition) : 0);
                const uint32 name_length = definition && definition->name
                    ? static_cast<uint32>(std::strlen(definition->name)) : 0u;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, name_length);
                if (name_length > 0)
                    hash = hgl::hash::FNV1aAppendBytes(
                        hash, definition->name, name_length);
                const uint32 glsl_length = definition && definition->glsl_code
                    ? static_cast<uint32>(std::strlen(definition->glsl_code)) : 0u;
                hash = hgl::hash::FNV1aAppendValueBytes(hash, glsl_length);
                if (glsl_length > 0)
                    hash = hgl::hash::FNV1aAppendBytes(
                        hash, definition->glsl_code, glsl_length);
            }

            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.ubo_count);
            for (uint32 i = 0; i < manifest.ubo_count; ++i)
                hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.ubos[i]);

            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.ssbo_count);
            for (uint32 i = 0; i < manifest.ssbo_count; ++i)
            {
                const auto &ssbo = manifest.ssbos[i];
                const uint32 name_length = static_cast<uint32>(std::strlen(ssbo.name));
                hash = hgl::hash::FNV1aAppendValueBytes(hash, name_length);
                hash = hgl::hash::FNV1aAppendBytes(hash, ssbo.name, name_length);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.ssbo_type);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.data_slot);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.stage_flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.required);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, ssbo.allow_fallback);
            }

            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.texture_count);
            for (uint32 i = 0; i < manifest.texture_count; ++i)
            {
                const auto &texture = manifest.textures[i];
                const uint32 name_length = static_cast<uint32>(std::strlen(texture.name));
                const uint32 glsl_length = static_cast<uint32>(std::strlen(texture.glsl_type));
                hash = hgl::hash::FNV1aAppendValueBytes(hash, name_length);
                hash = hgl::hash::FNV1aAppendBytes(hash, texture.name, name_length);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, glsl_length);
                hash = hgl::hash::FNV1aAppendBytes(hash, texture.glsl_type, glsl_length);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.semantic);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.slot);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.stage_flags);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.required);
                hash = hgl::hash::FNV1aAppendValueBytes(hash, texture.allow_fallback);
            }

            hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.texture_layer_count);
            for (uint32 i = 0; i < manifest.texture_layer_count; ++i)
                hash = hgl::hash::FNV1aAppendValueBytes(hash, manifest.texture_layers[i]);

            manifest.stable_hash = hash;
        }
    }

    bool BuildShaderResourceManifest(
        const GLSLCodeModuleID *root_modules,
        const uint32 root_module_count,
        ShaderResourceManifest &manifest,
        const GLSLCodeModuleRegistry *registry) noexcept
    {
        manifest = ShaderResourceManifest{};
        if (!root_modules && root_module_count > 0)
        {
            manifest.error = ShaderResourceManifestError::NullRootList;
            return false;
        }

        GLSLCodeModuleID visited_ids[MaxShaderResourceManifestCodeModules]{};
        VisitState states[MaxShaderResourceManifestCodeModules]{};
        uint32 visited_count = 0;

        for (uint32 i = 0; i < root_module_count; ++i)
        {
            if (!AddModule(root_modules[i], registry, visited_ids, states,
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
        case ShaderResourceManifestError::TextureCapacityExceeded: return "TextureCapacityExceeded";
        case ShaderResourceManifestError::TextureLayerCapacityExceeded: return "TextureLayerCapacityExceeded";
        case ShaderResourceManifestError::ResourceConflict: return "ResourceConflict";
        }

        return "Unknown";
    }
}
