// ──────────────────────────────────────────────────────────────────────────
// frag_forward_ubo.glsl — Conditional UBO includes for forward fragment shaders.
//
// Include this BEFORE #include SURFACE_FUNCTION_FILE so that surface
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

// Varying declarations + auto-defines MATERIAL_INSTANCE_ID_OVERRIDE
#include "common/varying_interface.glsl"
