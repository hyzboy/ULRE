#ifndef ULRE_COMPOSITOR_FRAG_FORWARD_UBO_GLSL
#define ULRE_COMPOSITOR_FRAG_FORWARD_UBO_GLSL

// ──────────────────────────────────────────────────────────────────────────
// frag_forward_ubo.glsl — Conditional UBO includes for forward fragment shaders.
//
// @sfm:require  UBO camera      (当 ENABLE_LIGHTING / NEEDS_CAMERA / HAS_CLIP_POS 时激活)
// @sfm:require  UBO sky         (当 ENABLE_LIGHTING / NEEDS_SKY / HAS_DIRECTION 时激活)
//
// Include this before the surface include so that surface
// functions referencing sky.* / camera.* can find the struct definitions.
// All UBO headers have include guards; re-including from frag_forward_main.glsl is safe.
// ──────────────────────────────────────────────────────────────────────────

#ifdef ENABLE_LIGHTING
#  include "common/ubo_camera.glsl"
#  include "common/ubo_sky.glsl"
#endif

#if defined(NEEDS_SKY) || defined(HAS_DIRECTION)
#  include "common/ubo_sky.glsl"
#endif

#if defined(NEEDS_CAMERA) || defined(HAS_CLIP_POS)
#  include "common/ubo_camera.glsl"
#endif

// Shared struct definitions (SurfaceInput, SurfaceOutput, SurfaceOutputExt)
#include "common/surface_interface.glsl"

// Varying declarations
#include "common/varying_fs.glsl"

#endif // ULRE_COMPOSITOR_FRAG_FORWARD_UBO_GLSL
