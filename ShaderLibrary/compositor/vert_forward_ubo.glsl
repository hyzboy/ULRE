#ifndef ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL
#define ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL

// ──────────────────────────────────────────────────────────────────────────
// vert_forward_ubo.glsl — Shared vertex shader prologue.
//
// Camera, transform, and material instance ID for all standard forward
// vertex shaders.  Include this after #version and any #extension lines.
//
// NEEDS_CAMERA    — define before including to pull in ubo_camera.glsl
// NEEDS_TRANSFORM — define before including to pull in ssbo_transform.glsl
// ──────────────────────────────────────────────────────────────────────────

#ifdef NEEDS_CAMERA
#include "common/ubo_camera.glsl"
#endif

#ifdef NEEDS_TRANSFORM
#include "common/ssbo_transform.glsl"
#endif

#if defined(PERMATERIAL_SET) && defined(MBI_ID_BINDING)
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#else
uint GetMaterialInstanceID()
{
	return gl_InstanceIndex;
}
#endif

#endif // ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL
