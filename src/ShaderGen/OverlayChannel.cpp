// OverlayChannel.cpp
// Stub implementation — SPV merge logic is TODO.

#include <hgl/shadergen/OverlayChannel.h>
#include <sstream>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// ApplyOverlays — STUB
// ---------------------------------------------------------------------------
std::vector<uint32_t> ApplyOverlays(const std::vector<uint32_t>& base_spv,
                                    const MatchedShaderSet&      /*matched*/)
{
    // TODO: implement actual SPIR-V patching for alpha / transition / policy overlays.
    // For now, return the base SPV unchanged so the rest of the pipeline can proceed.
    return base_spv;
}

// ---------------------------------------------------------------------------
// BuildAlphaOverlayDefines
// ---------------------------------------------------------------------------
std::string BuildAlphaOverlayDefines(const AlphaOverlay& overlay)
{
    std::ostringstream ss;
    ss << "#define ALPHA_MODE " << static_cast<int>(overlay.mode) << "\n";
    ss << "#define ALPHA_SOURCE " << static_cast<int>(overlay.source) << "\n";
    if (overlay.mode == AlphaOverlay::Mode::Masked ||
        overlay.mode == AlphaOverlay::Mode::Dither)
    {
        ss << "#define ALPHA_THRESHOLD " << overlay.threshold << "\n";
    }
    if (overlay.tex_slot != 0xFF)
        ss << "#define ALPHA_TEX_SLOT " << static_cast<int>(overlay.tex_slot) << "\n";
    return ss.str();
}

// ---------------------------------------------------------------------------
// BuildTransitionOverlayDefines
// ---------------------------------------------------------------------------
std::string BuildTransitionOverlayDefines(const TransitionOverlay& overlay)
{
    std::ostringstream ss;
    ss << "#define TRANSITION_ENABLED 1\n";
    ss << "#define TRANSITION_PROGRESS " << overlay.transition_progress << "\n";
    return ss.str();
}

// ---------------------------------------------------------------------------
// BuildPolicyOverlayDefines
// ---------------------------------------------------------------------------
std::string BuildPolicyOverlayDefines(const PolicyOverlay& overlay)
{
    if (overlay.transform_fn_path.empty())
        return {};
    std::ostringstream ss;
    ss << "#define POLICY_TRANSFORM_FN_PATH \"" << overlay.transform_fn_path << "\"\n";
    return ss.str();
}

} // namespace hgl::graph
