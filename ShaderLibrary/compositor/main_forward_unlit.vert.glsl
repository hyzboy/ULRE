#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    layout(location=POSITION_LOCATION) in vec3 Position;
#endif

layout(location=0) flat out uint fragMaterialInstanceID;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 l2w_mat = GetTransform();

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
#else
    vec3 pos = Position;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);
    gl_Position = camera.vp * worldPos;
}
