#include "compositor/vert_forward_ubo.glsl"

#define VARYING_STAGE_VERT
#define HAS_CLIP_POS
#define HAS_NORMAL
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    ivec2 tex_sz = textureSize(TextureHeight, 0);
    int W = tex_sz.x;

    int idx = gl_VertexIndex;
    ivec2 coord = ivec2(idx % W, idx / W);

    float h = texelFetch(TextureHeight, coord, 0).r;
    vec3 nrm = normalize(texelFetch(TextureNormal, coord, 0).xyz * 2.0 - 1.0);

    vec3 pos = vec3(float(coord.x), float(coord.y), h);

    mat4 transform_mat = GetTransform();
    vec4 wp = transform_mat * vec4(pos, 1.0);

    vec3 wn = normalize(mat3(transform_mat) * nrm);

    fragWorldNormal = wn;
    fragClipPos = camera.vp * wp;

    gl_Position = fragClipPos;
}
