// OverlayChannel.h
// Unified overlay channel for Alpha, DitherMask transition, and vertex policy overlays.
//
// DESIGN:
//   ApplyOverlays() is the main entry point.
//   Current implementation is a STUB — it records the overlay descriptors and
//   returns the base SPV unchanged.  The real SPV-merge logic is a TODO for a
//   later phase once the compositor assembler supports SPIR-V patching.
//
// USAGE:
//   std::vector<uint32_t> final_spv = ApplyOverlays(base_spv, matched_set);

#pragma once

#include <hgl/shadergen/MatchedShaderSet.h>
#include <vector>
#include <cstdint>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// ApplyOverlays
// ---------------------------------------------------------------------------
/// Compose overlays from `matched` on top of `base_spv`.
///
/// Overlay priority (applied in order):
///   1. PolicyOverlay  — vertex transform injection
///   2. AlphaOverlay   — alpha / discard / blend-mode specialisation
///   3. TransitionOverlay — dither-mask dissolve
///
/// @param base_spv   SPIR-V bytecode of the base compiled surface shader
/// @param matched    MatchedShaderSet carrying the overlay descriptors
/// @return           Composed SPIR-V (currently identical to base_spv until TODO is implemented)
std::vector<uint32_t> ApplyOverlays(const std::vector<uint32_t>& base_spv,
                                    const MatchedShaderSet&      matched);

// ---------------------------------------------------------------------------
// Individual overlay builders (return GLSL snippet paths / define strings)
// These are lower-level helpers used internally by ApplyOverlays.
// ---------------------------------------------------------------------------

/// Build the #define block for an AlphaOverlay (injected as GLSL preamble).
std::string BuildAlphaOverlayDefines(const AlphaOverlay& overlay);

/// Build the #define block for a TransitionOverlay.
std::string BuildTransitionOverlayDefines(const TransitionOverlay& overlay);

/// Build the #define block for a PolicyOverlay.
std::string BuildPolicyOverlayDefines(const PolicyOverlay& overlay);

} // namespace hgl::graph
