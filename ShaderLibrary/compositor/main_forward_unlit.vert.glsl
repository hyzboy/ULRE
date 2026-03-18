#version 450

// === Compositor Template: Forward Unlit VS ===
// Unlit 材质专用 — 仅需 Position，无 Normal/UV
//
// Descriptor binding 约定（Resort() 按字母序分配）：
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

// Vertex input — SSBO fetch or VBO
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    layout(location=0) in vec3 Position;
#endif

// ECS instance-rate attributes (dual ID)
#if TRANSFORM_ID_FROM_DESCRIPTOR
    #include "common/transform_id_buffer.glsl"
    TRANSFORM_ID_BUFFER;
    #define GET_TRANSFORM_ID()          FetchTransformID()
    layout(location=1) in uint MaterialInstanceID;
    #define GET_MATERIAL_INSTANCE_ID()  MaterialInstanceID
#elif GEOMETRY_FETCH_SSBO
    #define GET_TRANSFORM_ID()          gl_InstanceIndex
    #define GET_MATERIAL_INSTANCE_ID()  gl_InstanceIndex
#else
    layout(location=1) in uint TransformID;
    layout(location=2) in uint MaterialInstanceID;
    #define GET_TRANSFORM_ID()          TransformID
    #define GET_MATERIAL_INSTANCE_ID()  MaterialInstanceID
#endif

// Outputs to FS
layout(location=0) flat out uint fragMaterialInstanceID;

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
    fragMaterialInstanceID = GET_MATERIAL_INSTANCE_ID();
    gl_Position = camera.vp * worldPos;
}
