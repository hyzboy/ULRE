#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL

// Billboard camera-facing: always uses a vec3 position attribute (xy = quad offset, z unused).
#define POSITION_KIND 2
#include "common/vertex_input_position.glsl"

#include "compositor/vert_forward_ubo.glsl"

#define VARYING_STAGE_VERT
#define HAS_BILLBOARD_TEXCOORD
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 transform_mat = GetTransform();
    vec3 pos3 = GetPositionLocal();
    vec3 center = (transform_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 world_pos = center
                   + pos3.x * camera.billboard_right
                   + pos3.y * camera.billboard_up;

    fragTexCoord = vec2(pos3.x + 0.5, pos3.y * -1.0 + 0.5);

    gl_Position = camera.vp * vec4(world_pos, 1.0);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_DYNAMIC_VERT_GLSL
