#version 450

// === Compositor Template: Terrain Grid VS ===
// 无顶点 Position 输入 — 通过 gl_VertexID 生成网格坐标
// VS 中 texelFetch 采样高度图和法线图
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, viewport=1
//   Transform set=1 : l2w=0
//   Material  set=2 : TextureHeight=0, TextureNormal=1

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

#if TRANSFORM_ID_FROM_DESCRIPTOR
    #include "common/transform_id_buffer.glsl"
    TRANSFORM_ID_BUFFER;
    #define GET_TRANSFORM_ID() FetchTransformID()
#else
    layout(location=0) in uint TransformID;
    #define GET_TRANSFORM_ID() TransformID
#endif

// VS textures (Material set) — texelFetch 不需要 sampler
layout(set=MATERIAL_SET, binding=0) uniform sampler2D TextureHeight;
layout(set=MATERIAL_SET, binding=1) uniform sampler2D TextureNormal;

// Vertex attributes: TransformID only (no Position!)

// Output to FS
layout(location=0) out vec4 fragClipPos;
layout(location=1) out vec3 fragWorldNormal;

void main()
{
    // Get texture size to determine grid dimensions
    ivec2 tex_sz = textureSize(TextureHeight, 0);
    int W = tex_sz.x;

    // Grid coordinate from vertex id
    int idx = gl_VertexID;
    ivec2 coord = ivec2(idx % W, idx / W);

    // Sample height (R) and normal (RGB)
    float h = texelFetch(TextureHeight, coord, 0).r;
    vec3 nrm = normalize(texelFetch(TextureNormal, coord, 0).xyz * 2.0 - 1.0);

    // Build local position: X = u, Y = v, Z = height
    vec3 pos = vec3(float(coord.x), float(coord.y), h);

    // Transform to world and clip space
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 wp = l2w_mat * vec4(pos, 1.0);

    // Transform normal to world (approx; ignore non-uniform scale)
    vec3 wn = normalize(mat3(l2w_mat) * nrm);

    fragWorldNormal = wn;
    fragClipPos = camera.vp * wp;

    gl_Position = fragClipPos;
}
