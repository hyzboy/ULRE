#version 450

// === Compositor Template: Billboard Dynamic Size VS ===
// 动态尺寸 Billboard — 世界空间展开，面积不随距离变化
//
// Descriptor binding 约定（固定布局）：
//   Scene     set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   （无 Material set 在 VS 中使用）

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;

// Vertex attributes: Position
layout(location=0) in vec3 Position;

// Output to FS
layout(location=0) out vec2 fragTexCoord;

void main()
{
    mat4 l2w_mat = l2w.mats[ResolveTransformID(gl_InstanceIndex)];
    vec3 center = (l2w_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 world_pos = center
                   + Position.x * camera.billboard_right
                   + Position.y * camera.billboard_up;

    fragTexCoord = vec2(Position.x + 0.5, Position.y * -1.0 + 0.5);

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}
