#version 450


#include "common/ubo_camera.glsl"
#include "common/ssbo_transform.glsl"
#define MATERIAL_INSTANCE_ID_ONLY
#include "common/ssbo_material_instance.glsl"

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec4 fragClipPos;
layout(location=2) out vec3 fragWorldNormal;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    ivec2 tex_sz = textureSize(TextureHeight, 0);
    int W = tex_sz.x;

    int idx = gl_VertexID;
    ivec2 coord = ivec2(idx % W, idx / W);

    float h = texelFetch(TextureHeight, coord, 0).r;
    vec3 nrm = normalize(texelFetch(TextureNormal, coord, 0).xyz * 2.0 - 1.0);

    vec3 pos = vec3(float(coord.x), float(coord.y), h);

    mat4 l2w_mat = GetTransform();
    vec4 wp = l2w_mat * vec4(pos, 1.0);

    vec3 wn = normalize(mat3(l2w_mat) * nrm);

    fragWorldNormal = wn;
    fragClipPos = camera.vp * wp;

    gl_Position = fragClipPos;
}
