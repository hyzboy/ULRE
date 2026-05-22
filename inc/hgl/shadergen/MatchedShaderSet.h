// MatchedShaderSet.h
// Core runtime carrier produced by the Matcher.
// Replaces MaterialVariantRow as the live per-drawcall shader descriptor.
//
// STRUCTURE:
//   - surface_path           : winning surface .glsl (compositor input)
//   - quality_level          : quality tier used for this match
//   - render_phase           : phase this set was matched for
//   - lighting_model         : snapshotted from GlobalRenderConfig at match time
//   - sky_ambient_model      : snapshotted from GlobalRenderConfig at match time
//   - tex_layout             : per-slot sampler layout, derived from ResourceSupply
//   - vertex_transform_policy: snapshotted from recipe
//   - position_provider_id   : snapshotted from recipe
//   - alpha_overlay          : optional; built from MaterialRecipe::alpha_config
//   - transition_overlay     : optional; DitherMask dissolve overlay
//   - policy_overlay         : optional; extra vertex transform injection
//   - pass_override          : optional; used by PassShaderResolver for depth/shadow paths
//   - is_fallback            : true  → Checkerboard path; surfaces should not render normally

#pragma once

#include <hgl/mtl/RenderPhase.h>
#include <hgl/mtl/GlobalRenderConfig.h>   // LightingModel, SkyAmbientModel
#include <hgl/common/PositionProvider.h>  // PositionProviderId

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// AlphaOverlay — built from MaterialRecipe::alpha_config
// ---------------------------------------------------------------------------
struct AlphaOverlay {
    enum class Mode : uint8_t {
        Opaque = 0,
        Masked,        ///< discard by threshold
        Transparent,   ///< src-alpha blend
        Dither,        ///< ordered dither
        AlphaToCoverage,
    };

    enum class Source : uint8_t {
        ConstantOne = 0,  ///< alpha = 1.0
        BaseColorAlpha,   ///< alpha from base color texture .a channel
        SeparateTex,      ///< dedicated alpha texture
        PCGFn,            ///< procedurally computed (pcg_path non-empty)
    };

    Mode    mode      = Mode::Opaque;
    Source  source    = Source::ConstantOne;
    float   threshold = 0.5f;          ///< used for Masked / Dither
    uint8_t tex_slot  = 0xFF;          ///< 0xFF = unused
    std::string pcg_path;              ///< ShaderLibrary-relative; empty unless Source==PCGFn
};

// ---------------------------------------------------------------------------
// TransitionOverlay — DitherMask dissolve, typically for object transitions
// ---------------------------------------------------------------------------
struct TransitionOverlay {
    std::string dither_mask_path;      ///< PCG or texture path for the dissolve pattern
    float       transition_progress = 0.f; ///< 0..1
    uint32_t    active_phase_mask   = 0;   ///< bitmask of RenderPhase values where active
};

// ---------------------------------------------------------------------------
// PolicyOverlay — extra vertex transform injected on top of position provider
// ---------------------------------------------------------------------------
struct PolicyOverlay {
    std::string transform_fn_path;     ///< ShaderLibrary-relative GLSL snippet
    // Additional parameters may be injected via UBO push constants; left open for now.
};

// ---------------------------------------------------------------------------
// PassShaderOverride — used by PassShaderResolver for EarlyZ/Shadow/etc.
// ---------------------------------------------------------------------------
struct PassShaderOverride {
    std::string vs_path;  ///< Explicit VS override for this pass
    std::string fs_path;  ///< Explicit FS override; may be empty for depth-only
};

// ---------------------------------------------------------------------------
// ResourceSupply — describes what resources the draw call can provide
// ---------------------------------------------------------------------------
struct ResourceSupply {
    std::vector<std::string> available_textures;  ///< logical texture names
    std::vector<std::string> available_ubos;
    std::vector<std::string> available_ssbos;
    bool sky_available = false;
};

// ---------------------------------------------------------------------------
// TexSlotLayout — per-slot sampler type info, resolved from ResourceSupply
// ---------------------------------------------------------------------------
struct TexSlotLayout {
    uint8_t     slot       = 0;
    std::string name;              ///< logical name (e.g. "BaseColor", "Normal")
    bool        is_array   = false;
    bool        is_cube    = false;
};

// ---------------------------------------------------------------------------
// MatchedShaderSet — the resolved runtime descriptor for one draw's shaders
// ---------------------------------------------------------------------------
struct MatchedShaderSet {
    // ── Core match result ─────────────────────────────────────────────────
    std::string        surface_path;          ///< Winning surface .glsl path
    uint32_t           quality_level  = 0;    ///< Quality tier used for this match

    // ── Phase / global config snapshot ───────────────────────────────────
    hgl::mtl::RenderPhase   render_phase   = hgl::mtl::RenderPhase::ForwardOpaque;
    hgl::mtl::LightingModel lighting_model = hgl::mtl::LightingModel::PBR;
    hgl::mtl::SkyAmbientModel sky_ambient_model = hgl::mtl::SkyAmbientModel::SkyAtmosphere;

    // ── Resource layout (resolved from supply) ───────────────────────────
    std::vector<TexSlotLayout> tex_layout;

    // ── Geometry policy (snapshotted from recipe) ────────────────────────
    // vertex_transform_policy stored as opaque id; consumer casts to its enum.
    uint32_t           vertex_transform_policy = 0;
    PositionProviderId position_provider = PositionProviderId::Unknown;

    // ── Optional overlay channels ────────────────────────────────────────
    std::optional<AlphaOverlay>      alpha_overlay;
    std::optional<TransitionOverlay> transition_overlay;
    std::optional<PolicyOverlay>     policy_overlay;

    // ── Phase-level override (EarlyZ / Shadow / Velocity / VBuffer) ──────
    std::optional<PassShaderOverride> pass_override;

    // ── Fallback flag ─────────────────────────────────────────────────────
    bool is_fallback = false;  ///< true → Checkerboard; do not render as normal surface

    // ── Helpers ───────────────────────────────────────────────────────────
    bool IsValid()    const { return !is_fallback && !surface_path.empty(); }
    bool HasAlpha()   const { return alpha_overlay.has_value(); }
    bool HasPassOverride() const { return pass_override.has_value(); }
};

} // namespace hgl::graph
