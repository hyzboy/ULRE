#version 450

// === Compositor Template: Camera-Facing Policy VS ===
// PositionSource: quad local XY
// TransformPolicy: camera-facing in world space

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
#include "common/position_source_transform_policy.glsl"

// Vertex attributes: Position
layout(location=0) in vec3 Position;

// Output to FS
layout(location=0) out vec2 fragTexCoord;

void main()
{
    mat4 l2w_mat = l2w.mats[ResolveTransformID(gl_InstanceIndex)];
    vec3 local_pos = PositionSourceQuadLocal(Position);
    vec3 center = PositionSourceObjectOrigin(l2w_mat);
    vec3 world_pos = TransformPolicyCameraFacing(center,
                                                 local_pos,
                                                 camera.billboard_right,
                                                 camera.billboard_up);

    fragTexCoord = vec2(Position.x + 0.5, Position.y * -1.0 + 0.5);

    gl_Position = TransformPolicyApplyVP(camera.vp, world_pos);
}
