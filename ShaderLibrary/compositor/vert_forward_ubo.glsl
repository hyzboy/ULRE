#ifndef ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL
#define ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL

// ──────────────────────────────────────────────────────────────────────────
// vert_forward_ubo.glsl — Shared vertex shader prologue.
//
// Camera, transform, and material instance ID for all standard forward
// vertex shaders.  Include this after #version and any #extension lines.
// ──────────────────────────────────────────────────────────────────────────

#if !defined(ULRE_INSTANCE_INDEX)
#if defined(ULRE_MESH_SHADER_STAGE)
#define ULRE_INSTANCE_INDEX 0u
#else
#define ULRE_INSTANCE_INDEX gl_InstanceIndex
#endif
#endif

#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"

#if defined(PERMATERIAL_SET) && defined(MBI_ID_BINDING)
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#else
uint GetMaterialInstanceID()
{
	return ULRE_INSTANCE_INDEX;
}
#endif

#endif // ULRE_COMPOSITOR_VERT_FORWARD_UBO_GLSL
