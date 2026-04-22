#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialResourceManifest.h>
#include <string>

namespace hgl::graph::mtl
{
    bool CollectShaderAutoRequirements(const StaticMaterialDef &base_def,
                                       const std::string &shader_library_path,
                                       const std::string &vertex_glsl,
                                       const std::string &fragment_glsl,
                                       MaterialResourceManifest &out_requirements,
                                       std::string *diagnostics = nullptr);

    /// Builds a merged manifest from base_def and auto-scanned requirements.
    /// Equivalent to FromStaticDef(base_def).MergeKeepFirst(auto_requirements).
    MaterialResourceManifest MergeManifestWithAutoRequirements(
        const StaticMaterialDef &base_def,
        const MaterialResourceManifest &auto_requirements);
}
