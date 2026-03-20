#version 450


#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

#include "common/l2w_ssbo.glsl"
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    layout(location=POSITION_LOCATION) in vec3 Position;
#endif


void main()
{
    mat4 l2w_mat = GetTransform();

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
#else
    vec3 pos = Position;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);
    gl_Position = camera.vp * worldPos;
}
