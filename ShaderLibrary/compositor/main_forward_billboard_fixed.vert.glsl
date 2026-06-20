#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL

// Billboard fixed-size: always uses a vec3 position attribute (xy = quad offset, z = depth).
#define POSITION_KIND 2
#include "common/vertex_input_position.glsl"

#include "compositor/vert_forward_ubo.glsl"
#include "common/ubo_viewport.glsl"

#define VARYING_STAGE_VERT
#define HAS_POSITION
#define HAS_TEXCOORD
#define HAS_BILLBOARD_TEXCOORD
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();

    mat4 transform_mat = GetTransform();
    vec3 pos3 = GetPositionLocal();
    vec2 billboard_size_pixels = vec2(64.0, 64.0);
    vec2 transform_scale = vec2(length(transform_mat[0].xyz), length(transform_mat[1].xyz));
    billboard_size_pixels *= max(transform_scale, vec2(0.0001, 0.0001));

    vec2 psize = billboard_size_pixels / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * transform_mat * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + pos3.xy * psize;

    vec2 uv = vec2(pos3.x + 0.5, pos3.y + 0.5);
    vec4 world_pos = transform_mat * vec4(0.0, 0.0, 0.0, 1.0);

    fragWorldPos = world_pos.xyz;
    fragUV0 = uv;
    fragTexCoord = uv;

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
