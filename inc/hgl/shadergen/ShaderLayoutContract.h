#pragma once

#include <string>
#include <vector>

namespace hgl::graph
{
    /// One resolved layout macro binding: macro_name → integer value.
    struct ShaderLayoutEntry
    {
        std::string macro_name;     ///< e.g. "POSITION_LOCATION", "SCENE_SET", "CAMERA_BINDING"
        int         value = -1;
    };

    /**
     * Holds all auto-resolved layout numbers produced by BuildShaderLayoutContract().
     *
     * Three buckets:
     *   - vertex_locations    : layout(location=N) assignments for vertex inputs
     *   - descriptor_sets     : layout(set=N) assignments for descriptor sets
     *   - descriptor_bindings : layout(binding=N) assignments for individual descriptors
     *
     * The contract is built after MaterialDescriptorInfo::Resort() has run (final
     * set/binding numbers are authoritative) and after ShaderCreateInfoVertex::AddInput()
     * has assigned locations (location = insertion order).
     */
    struct ShaderLayoutContract
    {
        std::vector<ShaderLayoutEntry> vertex_locations;
        std::vector<ShaderLayoutEntry> descriptor_sets;
        std::vector<ShaderLayoutEntry> descriptor_bindings;

        void Clear()
        {
            vertex_locations.clear();
            descriptor_sets.clear();
            descriptor_bindings.clear();
        }

        bool Empty() const
        {
            return vertex_locations.empty()
                && descriptor_sets.empty()
                && descriptor_bindings.empty();
        }
    };

}  // namespace hgl::graph
