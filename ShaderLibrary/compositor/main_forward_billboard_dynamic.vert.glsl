#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL

// Billboard camera-facing.
// Mesh:   unit quad, Position2D in [-0.5, 0.5]^2, with TexCoord.
// Scale:  Transform.scale.xy = world-space size (meters).
//         length(M[0].xyz) = width,  length(M[1].xyz) = height.
//         Transform rotation is intentionally ignored (billboard always faces camera).
#define POSITION_KIND 1
#include "common/vertex_input_position.glsl"

#include "compositor/vert_forward_ubo.glsl"

#define VARYING_STAGE_VERT
#define HAS_TEXCOORD
#include "common/varying_interface.glsl"

layout(location=3) in vec2 in_TexCoord;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 M = GetTransform();
    vec2 local_offset = GetPositionLocal();          // in [-0.5, 0.5]

    vec3 center = M[3].xyz;                          // translation only
    float sx = length(M[0].xyz);                     // world-space width  in meters
    float sy = length(M[1].xyz);                     // world-space height in meters

    vec3 world_pos = center
                   + (local_offset.x * sx) * camera.billboard_right
                   + (local_offset.y * sy) * camera.billboard_up;

    fragUV0 = in_TexCoord;

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL
