// Matcher.cpp
// Implementation of the shader matcher.
// Phase-specific compositor/pipeline selection is NOT handled here;
// that is the responsibility of PassShaderResolver (upper layer).

#include <hgl/shadergen/Matcher.h>
#include <hgl/mtl/MaterialPresetTable.h>
#include <hgl/mtl/GlobalRenderConfig.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <cstdio>
#include <algorithm>

namespace hgl::graph {

// ── Matcher ───────────────────────────────────────────────────────────────────

bool Matcher::CheckSurfaceCompatibility(const char*                 surface_path,
                                        const GeometryVertexFormat& /*geometry*/,
                                        const ResourceSupply&       supply,
                                        std::string*                /*failure_reason*/) {
    if (!surface_path) {
        // Bespoke VS+FS preset (e.g. Text2D): no surface fn to validate
        return true;
    }
    // TODO: When ProviderManifest is available, validate VA/resource requirements here
    return true;
}

MatchedShaderSet Matcher::GetCheckerboardFallback() {
    MatchedShaderSet result;
    result.surface_id   = mtl::SurfaceId::Checkerboard;
    result.surface_path = mtl::GetSurfacePath(mtl::SurfaceId::Checkerboard);
    result.is_fallback  = true;
    return result;
}

MatchedShaderSet Matcher::Resolve(const mtl::MaterialRecipe& recipe,
                                   const GeometryVertexFormat&  geometry,
                                   const ResourceSupply&        supply) {
    const uint8_t current_quality =
        static_cast<uint8_t>(hgl::mtl::GlobalRenderConfig::Instance().GetQualityLevel());

    std::vector<std::string> failure_log;

    for (uint8_t q = current_quality; q >= 1; --q) {
        const mtl::PresetQualityEntry* entry =
            mtl::MaterialPresetTable::Lookup(recipe.preset, q);
        if (!entry) {
            failure_log.push_back("Quality " + std::to_string(q) +
                                  ": No preset table entry found");
            if (q == 1) break;
            continue;
        }

        const char* surface_path = mtl::GetSurfacePath(entry->surface);

        std::string failure_reason;
        if (CheckSurfaceCompatibility(surface_path, geometry, supply, &failure_reason)) {
            MatchedShaderSet result;
            result.surface_id   = entry->surface;
            result.surface_path = surface_path ? surface_path : "";
            result.vs_path      = entry->vs_override ? entry->vs_override : "";
            result.fs_path      = entry->fs_override ? entry->fs_override : "";
            result.is_fallback  = false;

            if (q < current_quality) {
                std::fprintf(stderr,
                    "[Matcher] Quality degraded from %u to %u for preset %u\n",
                    current_quality, q, static_cast<unsigned>(recipe.preset));
            }
            return result;
        }

        failure_log.push_back("Quality " + std::to_string(q) + ": " + failure_reason);
        if (q == 1) break;
    }

    // Total failure
    std::fprintf(stderr,
        "\n════════════════════════════════════════════════════════════════\n"
        "[Matcher] SHADER RESOLUTION FAILED\n"
        "════════════════════════════════════════════════════════════════\n"
        "Preset:  %u\n"
        "Quality: %u (attempted degradation down to 1)\n"
        "────────────────────────────────────────────────────────────────\n",
        static_cast<unsigned>(recipe.preset), current_quality);
    for (const auto& msg : failure_log)
        std::fprintf(stderr, "  * %s\n", msg.c_str());
    std::fprintf(stderr,
        "════════════════════════════════════════════════════════════════\n\n");

    return GetCheckerboardFallback();
}

} // namespace hgl::graph
