#pragma once

#include<hgl/type/String.h>
#include<string>
#include<unordered_map>
#include<functional>

namespace hgl
{
    namespace graph
    {
        class GraphicsContext;
        class Sampler;
        class Texture2DArray;
        class ShaderMaterialProgram;
        class DomainResourceBinding;
        class Primitive;
    }
}

namespace hgl::graph
{
    /**
     * TextureDomainRegistry
     *
     * Generic service for texture-array / domain batching resource management.
     * Manages per-domain Texture2DArray, Sampler, ShaderMaterialProgram and
     * DomainResourceBinding, independently of any mesh or billboard type.
     *
     * Intended usage:
     *   - Call RegisterTexture() each frame for any entity that uses a domain tag.
     *   - Call EnsureResources() once per frame (or on demand) to build/rebuild
     *     any dirty texture arrays.
     *   - Call GetEntry() to retrieve the ready resources for a domain.
     */
    class TextureDomainRegistry
    {
    public:

        /// Per-domain resources for texture-array batching.
        struct DomainEntry
        {
            std::string                                 domain_tag;
            graph::Texture2DArray*                      texture_array   = nullptr;
            graph::ShaderMaterialProgram*               material        = nullptr;
            graph::DomainResourceBinding*               dmb             = nullptr;
            graph::Sampler*                             sampler         = nullptr;
            graph::Primitive*                           primitive       = nullptr;
            uint32_t                                    max_layers      = 0;
            uint32_t                                    used_layers     = 0;
            std::unordered_map<hgl::OSString, uint32_t> path_to_layer;
            bool                                        dirty           = false;
        };

    private:

        static std::unordered_map<std::string, DomainEntry> s_entries;

    public:

        /// Register a texture path in a domain; returns its layer index.
        /// If already registered, returns the existing layer.
        /// Returns -1 if the domain is at capacity.
        static int RegisterTexture(const std::string& domain_tag,
                                   const hgl::OSString& texture_path);

        /// Get domain entry, or nullptr if domain has not been registered yet.
        static DomainEntry* GetEntry(const std::string& domain_tag);

        /// Iterate over all registered domain entries.
        static void ForEach(const std::function<void(const std::string&, DomainEntry&)>& fn);

        /// Build or rebuild texture arrays for all dirty domains.
        /// The caller is responsible for providing a valid GraphicsContext,
        /// and optionally a callback to create the ShaderMaterialProgram / DMB.
        ///
        /// @param gc               Active GraphicsContext (must not be nullptr).
        /// @param build_material   Optional callback: given domain_tag, fills DomainEntry's
        ///                         material and dmb fields.  If nullptr the caller must
        ///                         ensure material / dmb are already present or will be set
        ///                         separately.
        /// @return true if all dirty domains were built successfully.
        static bool EnsureResources(
            GraphicsContext* gc,
            std::function<bool(const std::string& domain_tag, DomainEntry& entry, GraphicsContext* gc)>
                build_material = nullptr);

        /// Release all resources for all domains (call on shutdown).
        static void ReleaseAll(GraphicsContext* gc);
    };

}//namespace hgl::graph
