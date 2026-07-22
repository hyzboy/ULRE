#version 450

// === Compositor Template: Forward Opaque VS ===
// 自动生成 — 不要手动编辑此文件
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO declarations (shared with all shaders)
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO — camera-relative local-to-world matrices, indexed by TransformID
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
DATA_INDEX_ROWS_SSBO;
// TEXTURE_LAYER_ROWS 不在 VS 中声明，由 FS surface file 负责（bindless 多槽查找）

#if GEOMETRY_FETCH_SSBO
    // SSBO 顶点获取
    #include "common/vertex_fetch_ssbo.glsl"
#else
    // VBO 顶点获取
    layout(location=0) in vec3 inPosition;
    layout(location=1) in vec3 inNormal;
    layout(location=2) in vec2 inUV0;
#endif

#define GET_TRANSFORM_ID()          ResolveTransformID(gl_InstanceIndex)
#define GET_DATA_INDEX_ID()         ResolveDataIndexID(gl_InstanceIndex)
#define GET_TEXTURE_LAYER_ID()      GET_DATA_INDEX_ID() // bindless: share texture row by data-index indirection

layout(location=0) out vec3 fragWorldPos;
layout(location=1) out vec3 fragWorldNormal;
layout(location=2) out vec2 fragUV0;
layout(location=3) flat out uint fragDataIndexID;
layout(location=4) flat out uint fragTextureLayerID;

void main()
{
    uint transformID = GET_TRANSFORM_ID();
    mat4 l2w_mat = l2w.mats[transformID];

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
    vec3 normal = FetchNormal(gl_VertexIndex);
    vec2 uv0 = FetchUV0(gl_VertexIndex);
#else
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec2 uv0 = inUV0;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);   // camera-relative world position
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(l2w_mat) * normal;
    fragUV0 = uv0;
    fragDataIndexID = GET_DATA_INDEX_ID();
    fragTextureLayerID = GET_TEXTURE_LAYER_ID();

    gl_Position = camera.vp * worldPos;   // vp 已是 camera-relative
}
