#version 450

// === Compositor Template: Fixed Screen-Scale Policy VS ===
// PositionSource: quad local XY
// TransformPolicy: fixed screen-space scale

#extension GL_EXT_scalar_block_layout : require

// Scene UBOs
#define VIEWPORT_BINDING 2
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_VIEWPORT_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
#include "common/position_source_transform_policy.glsl"

// MI SSBO (Material set, VS only)
#include "common/material_instance_ssbo.glsl"
struct MaterialInstance {
    uvec2 BillboardSize;
};
MI_SSBO_SCALAR;
DATA_INDEX_ROWS_SSBO;
TEXTURE_LAYER_ROWS_SSBO;

// Vertex attributes: Position
layout(location=0) in vec3  Position;

// Output to FS
layout(location=0) out vec2 fragTexCoord;

MaterialInstance GetMI() { return mtl.mi[ResolveDataIndexID(gl_InstanceIndex)]; }

void main()
{
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * l2w.mats[ResolveTransformID(gl_InstanceIndex)] * vec4(0.0, 0.0, 0.0, 1.0);
    vec3 local_pos = PositionSourceQuadLocal(Position);

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = TransformPolicyFixedScreenScale(center_clip, local_pos.xy, psize);
}
