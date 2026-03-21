// ──────────────────────────────────────────────────────────────────────────
// vert_forward_ubo.glsl — Shared vertex shader prologue.
//
// Camera, transform, and material instance ID for all standard forward
// vertex shaders.  Include this after #version and any #extension lines.
// ──────────────────────────────────────────────────────────────────────────

#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
