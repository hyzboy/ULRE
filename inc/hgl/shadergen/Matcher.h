// Matcher.h
// Shader matcher: resolves (Recipe + Geometry + ResourceSupply + RenderPhase) → MatchedShaderSet
//
// DESIGN:
//   1. If IsManagedByPassResolver(phase) → delegate to PassShaderResolver (EarlyZ/Shadow/etc.)
//   2. Lookup preset table candidates for (Preset, QualityLevel..1, Phase) → ordered list
//   3. For each candidate, parse SFM metadata, check VA/tex/ubo/ssbo/sky requirements
//   4. Return first passing candidate (highest quality wins)
//   5. On total failure, return CheckerboardFallback with detailed log
//
// SEE ALSO:
//   PassShaderResolver — handles EarlyZ / ShadowCaster / Velocity / VisibilityBuffer
//   OverlayChannel     — applies alpha / transition / policy overlays to composed SPV

#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialPresetTable.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/RenderPhase.h>
#include <hgl/shadergen/MatchedShaderSet.h>
#include <string>
#include <vector>

namespace hgl::graph {

/// Placeholder geometry vertex format — replace with real geometry type once unified.
struct GeometryVertexFormat {
    bool HasAll() const { return true; } // stub: assume all VA available
};

/// Shader matcher
class Matcher {
public:
    /// Resolve a MatchedShaderSet for the given recipe, geometry, resource supply, and render phase.
    ///
    /// Phases managed by PassShaderResolver (EarlyZ / ShadowCaster / Velocity / VisibilityBuffer)
    /// are forwarded there automatically; all other phases go through quality-degradation matching.
    static MatchedShaderSet Resolve(const mtl::MaterialRecipe& recipe,
                                    const GeometryVertexFormat& geometry,
                                    const ResourceSupply&       supply,
                                    hgl::mtl::RenderPhase       phase = hgl::mtl::RenderPhase::ForwardOpaque);

private:
    /// Check if a surface GLSL path satisfies VA/resource/sky requirements from SFM annotations.
    static bool CheckSurfaceCompatibility(const char*                 surface_path,
                                          const GeometryVertexFormat& geometry,
                                          const ResourceSupply&       supply,
                                          hgl::mtl::RenderPhase       phase,
                                          std::string*                failure_reason);

    /// Compose and return a MatchedShaderSet from a winning surface path.
    static MatchedShaderSet ComposeResult(const char*                surface_path,
                                          uint32_t                   quality_level,
                                          hgl::mtl::RenderPhase      phase,
                                          const mtl::MaterialRecipe& recipe,
                                          const ResourceSupply&      supply);

    /// Return the CheckerboardFallback shader set and log diagnostics.
    static MatchedShaderSet GetCheckerboardFallback(mtl::MaterialPreset preset,
                                                    hgl::mtl::RenderPhase phase,
                                                    const std::vector<std::string>& reasons);
};

} // namespace hgl::graph
