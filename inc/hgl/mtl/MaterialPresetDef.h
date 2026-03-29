#pragma once

#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    /// MaterialPresetDef — high-level semantic material package loaded from a .matpreset JSON file.
    /// A preset wraps a LOD degradation chain of MaterialDef names.
    /// At the current stage, lod_chain typically has exactly one entry (1:1 mapping).
    struct MaterialPresetDef
    {
        std::string name;               // e.g. "PureColor3D"

        /// LOD degradation chain: index 0 = highest quality, higher indices = simpler fallbacks.
        /// Each entry is a MaterialDef name (matches the "name" field in the corresponding .mat file).
        std::vector<std::string> lod_chain;

        /// Serialised JSON text of the default MaterialInstance parameters (optional).
        /// Stored as raw string to avoid propagating nlohmann_json into consumer headers.
        std::string default_params_json; // e.g. "{\"roughness\":0.6}"

        /// Default create-config overrides (reduces boilerplate in example code).
        struct DefaultConfig
        {
            bool with_camera = true;
            bool with_sky    = false;
            bool with_l2w    = true;
        } default_config;

        bool IsValid() const
        {
            return !name.empty() && !lod_chain.empty();
        }

        /// Returns the primary (highest-quality) MaterialDef name.
        const std::string& GetPrimaryMaterialName() const
        {
            return lod_chain.front();
        }
    };

} // namespace hgl::graph::mtl
