#pragma once
/// MaterialRenderState.h - per-material render-state description (Phase A)
///
/// Three-layer composition model:
///   Recipe.default_render_state          asset-level default, offline bake input
///   PrimitiveComponent.user_rs_override   persistent per-primitive override (logic/player settings)
///   PrimitiveComponent.transition_state   frame-level injected layer (LOD/MeshBlend/Occlude systems)
///
/// ResolveSystem calls ComposeEffectiveRenderState() to flatten the three layers into one effective
/// value before looking up the material key.

#include <hgl/CoreType.h>
#include <hgl/mtl/RenderAlphaMode.h>

namespace hgl::graph::mtl
{

// ---------------------------------------------------------------------------
// Cull mode
// ---------------------------------------------------------------------------
enum class CullMode : uint8
{
    Back  = 0,  ///< default: cull back faces
    Front,      ///< cull front faces (shadow pass etc.)
    None,       ///< double-sided
};

// ---------------------------------------------------------------------------
// Depth compare operation
// ---------------------------------------------------------------------------
enum class DepthOp : uint8
{
    Less        = 0,
    LessEqual,          ///< recommended for most opaque objects
    Equal,
    Greater,
    GreaterEqual,
    Always,             ///< disable depth test (transparent / UI)
    Never,
};

// ---------------------------------------------------------------------------
// Material render state (serializable; used as both offline bake input and
// runtime override data)
// ---------------------------------------------------------------------------
struct MaterialRenderState
{
    RenderAlphaMode blend       = RenderAlphaMode::Opaque;
    CullMode        cull        = CullMode::Back;
    bool            depth_test  = true;
    bool            depth_write = true;
    DepthOp         depth_op    = DepthOp::LessEqual;
    float           alpha_ref   = 0.5f;     ///< only relevant in Masked mode

    bool operator==(const MaterialRenderState& o) const
    {
        return blend       == o.blend
            && cull        == o.cull
            && depth_test  == o.depth_test
            && depth_write == o.depth_write
            && depth_op    == o.depth_op
            && alpha_ref   == o.alpha_ref;
    }
    bool operator!=(const MaterialRenderState& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Frame-level transition overlay (injected by systems; not set directly by user)
// ---------------------------------------------------------------------------
struct TransitionState
{
    /// 0.0 = full DitherMask; 1.0 = no overlay (default).
    /// Effective only when PresetDemand::supports_dither_mask_transition == true.
    float dither_alpha = 1.0f;

    bool IsActive() const { return dither_alpha < 1.0f; }
    void Reset()          { dither_alpha = 1.0f; }
};

// ---------------------------------------------------------------------------
// Offline bake hints (Recipe field; tells the baker which additional pass
// SPV variants to generate)
// ---------------------------------------------------------------------------
struct PrecompileHints
{
    bool also_dither_mask  = false;   ///< bake a Dither overlay variant (even if default is Opaque)
    bool also_shadow_only  = true;    ///< bake ShadowCast variant (position-only)
    bool also_depth_only   = true;    ///< bake DepthPrepass variant
    bool also_transparent  = false;   ///< bake Transparent variant (if default is Opaque)
};

// ---------------------------------------------------------------------------
// Three-layer composition helper (call at Resolver entry)
// ---------------------------------------------------------------------------

/// Flatten three layers into one effective MaterialRenderState.
/// @param base                   from Recipe.default_render_state
/// @param user_override          if non-null, overrides base entirely
/// @param transition             frame-level overlay (translated to blend=Dither when preset supports it)
/// @param preset_supports_dither read from PresetDemand by caller
inline MaterialRenderState ComposeEffectiveRenderState(
    const MaterialRenderState&  base,
    const MaterialRenderState*  user_override,
    const TransitionState&      transition,
    bool                        preset_supports_dither) noexcept
{
    MaterialRenderState rs = user_override ? *user_override : base;
    if (transition.IsActive() && preset_supports_dither)
        rs.blend = RenderAlphaMode::Dither;
    return rs;
}

} // namespace hgl::graph::mtl
