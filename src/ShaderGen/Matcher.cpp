// Matcher.cpp
// Implementation of the shader matcher.
// Phase-specific pass shaders (EarlyZ / Shadow / Velocity / VisibilityBuffer)
// are forwarded to PassShaderResolver; all other phases go through quality-
// degradation surface matching.

#include <hgl/shadergen/Matcher.h>
#include <hgl/shadergen/PassShaderResolver.h>
#include <hgl/mtl/MaterialPresetTable.h>
#include <hgl/mtl/GlobalRenderConfig.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <cstdio>
#include <string>
#include <vector>

namespace hgl::graph {

// ── CheckSurfaceCompatibility ─────────────────────────────────────────────────

bool Matcher::CheckSurfaceCompatibility(const char*                 surface_path,
                                        const GeometryVertexFormat& /*geometry*/,
                                        const ResourceSupply&       /*supply*/,
                                        hgl::graph::mtl::RenderPhase       /*phase*/,
                                        std::string*                /*failure_reason*/)
{
    if (!surface_path) {
        // Bespoke VS+FS preset (e.g. Text2D): no surface fn to validate
        return true;
    }
    // TODO: parse SFM annotations and check:
    //   sfm.supports_phase ∋ phase
    //   sfm.va_required ⊆ geometry.va ∪ sfm.va_derive
    //   sfm.tex_required ⊆ supply.available_textures
    //   sfm.ubo_required ⊆ supply.available_ubos
    //   sfm.needs_sky → supply.sky_available
    return true;
}

// ── ComposeResult ─────────────────────────────────────────────────────────────

MatchedShaderSet Matcher::ComposeResult(const char*                surface_path,
                                        uint32_t                   quality_level,
                                        hgl::graph::mtl::RenderPhase      phase,
                                        const mtl::MaterialRecipe& recipe,
                                        const ResourceSupply&      /*supply*/)
{
    const auto& cfg = hgl::graph::mtl::GlobalRenderConfig::Instance();

    MatchedShaderSet result;
    result.surface_path       = surface_path ? surface_path : "";
    result.quality_level      = quality_level;
    result.render_phase       = phase;
    result.lighting_model     = cfg.GetLightingModel();
    result.sky_ambient_model  = cfg.GetSkyAmbientModel();
    // TODO: result.position_provider once MaterialRecipe gains that field
    // vertex_transform_policy: keep as opaque uint32; consumer casts
    result.vertex_transform_policy = static_cast<uint32_t>(recipe.vertex_policy);
    result.is_fallback        = false;

    // TODO: build tex_layout from supply.available_textures per-slot
    // TODO: build alpha_overlay from recipe.alpha_config
    // TODO: build transition_overlay if recipe has dither transition hint

    return result;
}

// ── GetCheckerboardFallback ───────────────────────────────────────────────────

MatchedShaderSet Matcher::GetCheckerboardFallback(mtl::MaterialPreset             preset,
                                                  hgl::graph::mtl::RenderPhase          phase,
                                                  const std::vector<std::string>& reasons)
{
    std::fprintf(stderr,
        "\n\033[1;31m"
        "════════════════════════════════════════════════════════════════\n"
        "[Matcher] SHADER RESOLUTION FAILED — CHECKERBOARD FALLBACK\n"
        "════════════════════════════════════════════════════════════════\033[0m\n"
        "  Preset : %u\n"
        "  Phase  : %s\n"
        "  Reasons:\n",
        static_cast<unsigned>(preset),
        hgl::graph::mtl::GetRenderPhaseName(phase));
    for (const auto& r : reasons)
        std::fprintf(stderr, "    * %s\n", r.c_str());
    std::fprintf(stderr,
        "\033[1;31m"
        "════════════════════════════════════════════════════════════════\033[0m\n\n");

    MatchedShaderSet result;
    result.surface_path  = mtl::GetSurfacePath(mtl::SurfaceId::Checkerboard);
    result.render_phase  = phase;
    result.is_fallback   = true;
    return result;
}

// ── Resolve ───────────────────────────────────────────────────────────────────

MatchedShaderSet Matcher::Resolve(const mtl::MaterialRecipe& recipe,
                                   const GeometryVertexFormat&  geometry,
                                   const ResourceSupply&        supply,
                                   hgl::graph::mtl::RenderPhase        phase)
{
    // Delegate pass-specific phases to PassShaderResolver
    if (IsManagedByPassResolver(phase)) {
        MatchedShaderSet result;
        result.render_phase  = phase;
        result.is_fallback   = false;
        // TODO: pass alpha_overlay once recipe.alpha_config is available
        result.pass_override = PassShaderResolver::Resolve(
            phase,
            static_cast<uint32_t>(recipe.vertex_policy),
            PositionProviderId::Unknown); // TODO: recipe.position_provider
        return result;
    }

    const uint8_t current_quality =
        static_cast<uint8_t>(hgl::graph::mtl::GlobalRenderConfig::Instance().GetQualityLevel());

    std::vector<std::string> failure_log;

    for (uint8_t q = current_quality; q >= 1; --q) {
        const mtl::PresetQualityEntry* entry =
            mtl::MaterialPresetTable::Lookup(recipe.preset, q, phase);
        if (!entry) {
            failure_log.push_back("Quality " + std::to_string(q) +
                                  ": No preset table entry");
            if (q == 1) break;
            continue;
        }

        const char* surface_path = mtl::GetSurfacePath(entry->surface);

        std::string failure_reason;
        if (CheckSurfaceCompatibility(surface_path, geometry, supply, phase, &failure_reason)) {
            if (q < current_quality) {
                std::fprintf(stderr,
                    "[Matcher] Quality degraded %u → %u for preset %u phase %s\n",
                    current_quality, q,
                    static_cast<unsigned>(recipe.preset),
                    hgl::graph::mtl::GetRenderPhaseName(phase));
            }
            return ComposeResult(surface_path, q, phase, recipe, supply);
        }

        failure_log.push_back("Quality " + std::to_string(q) + ": " + failure_reason);
        if (q == 1) break;
    }

    return GetCheckerboardFallback(recipe.preset, phase, failure_log);
}

} // namespace hgl::graph

