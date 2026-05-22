// Matcher.h
// Shader matcher: resolves (Recipe + Geometry + ResourceSupply) → MatchedShaderSet
//
// DESIGN:
//   1. Lookup preset table entry for (Preset, QualityLevel)  → PresetQualityEntry
//   2. GetSurfacePath(SurfaceId)                             → GLSL path
//   3. Parse SFM metadata via ProviderManifestRegistry
//   4. Check VA requirements ⊆ geometry VA supply
//   5. Check tex/ubo/ssbo requirements ⊆ resource supply
//   6. On failure, degrade quality level and retry
//   7. On total failure, return CheckerboardFallback and log diagnostics
//
// NOTE:
//   RenderPhase is NOT a parameter here.
//   Phase-specific compositor template and pipeline-state selection is handled
//   by the upper-layer PassShaderResolver, not by the surface matcher.

#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialPresetTable.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <string>
#include <vector>

namespace hgl::graph {

/// Placeholder geometry vertex format — replace with real geometry type once unified.
struct GeometryVertexFormat {
    bool HasAll() const { return true; } // stub: assume all VA available
};

/// Resource supply descriptor
struct ResourceSupply {
    std::vector<std::string> available_textures;
    std::vector<std::string> available_ubos;
    std::vector<std::string> available_ssbos;
    bool sky_available = false;
};

/// Matched shader set result
struct MatchedShaderSet {
    mtl::SurfaceId surface_id  = mtl::SurfaceId::None; ///< Resolved surface enum
    std::string    surface_path;                        ///< Resolved GLSL path
    std::string    vs_path;                             ///< Empty = compositor default
    std::string    fs_path;                             ///< Empty = compositor default
    bool           is_fallback = false;
};

/// Shader matcher
class Matcher {
public:
    /// Resolve a shader set for the given recipe, geometry, and resource supply.
    /// Phase-specific compositor/pipeline selection is handled separately by PassShaderResolver.
    static MatchedShaderSet Resolve(const mtl::MaterialRecipe& recipe,
                                    const GeometryVertexFormat& geometry,
                                    const ResourceSupply&       supply);

private:
    /// Check if a surface GLSL path satisfies VA/resource/sky requirements
    static bool CheckSurfaceCompatibility(const char*                 surface_path,
                                          const GeometryVertexFormat& geometry,
                                          const ResourceSupply&       supply,
                                          std::string*                failure_reason);

    /// Return the CheckerboardFallback shader set
    static MatchedShaderSet GetCheckerboardFallback();
};

} // namespace hgl::graph
