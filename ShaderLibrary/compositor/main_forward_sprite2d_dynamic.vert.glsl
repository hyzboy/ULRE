#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_DYNAMIC_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_DYNAMIC_VERT_GLSL

// Sprite2DCameraFacing — world-space size, camera-facing billboard.
// Position is vec2 (unit square [-0.5, 0.5] in both axes).
// The object transform provides center position and scale.

#include "compositor/vert_forward_ubo.glsl"
layout(location=POSITION_LOCATION) in vec2 Position;

#define VARYING_STAGE_VERT
#define HAS_BILLBOARD_TEXCOORD
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 transform_mat = GetTransform();
    vec3 center = (transform_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 world_pos = center
                   + Position.x * camera.billboard_right
                   + Position.y * camera.billboard_up;

    fragTexCoord = vec2(Position.x + 0.5, Position.y * -1.0 + 0.5);

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_DYNAMIC_VERT_GLSL
