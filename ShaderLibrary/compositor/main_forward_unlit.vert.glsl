#version 450

// === Compositor Template: Forward Unlit VS ===
// Unlit 材质专用 — 仅需 Position，无 Normal/UV
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO — camera-relative local-to-world matrices, indexed by TransformID
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
DATA_INDEX_ROWS_SSBO;
TEXTURE_LAYER_ROWS_SSBO;

// Vertex input — SSBO fetch or VBO
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    layout(location=0) in vec3 Position;
#endif

#define GET_TRANSFORM_ID()          ResolveTransformID(gl_InstanceIndex)
#define GET_DATA_INDEX_ID()         ResolveDataIndexID(gl_InstanceIndex)
#define GET_TEXTURE_LAYER_ID()      GET_DATA_INDEX_ID()  // share the same indirection as mtl

// Outputs to FS
layout(location=0) flat out uint fragDataIndexID;
layout(location=1) flat out uint fragTextureLayerID;

void main()
{
    uint transformID = GET_TRANSFORM_ID();
    mat4 l2w_mat = l2w.mats[transformID];

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
#else
    vec3 pos = Position;
#endif

    vec4 worldPos = l2w_mat * vec4(pos, 1.0);
    fragDataIndexID = GET_DATA_INDEX_ID();
    fragTextureLayerID = GET_TEXTURE_LAYER_ID();
    gl_Position = camera.vp * worldPos;
}
