#version 450

// === Compositor Template: Forward Unlit VS (Luminance, vec3 Position) ===
// 顶点亮度材质 — 用于 VertexLuminance3D (vec3)
//
// Descriptor binding 约定（固定布局）：
//   Scene    set=0 : camera=0, viewport=2
//   Transform set=1 : l2w=0
//   Material set=2 : mtl=0

// Scene UBO
#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;

// L2W SSBO
#include "common/l2w_ssbo.glsl"
L2W_SSBO;
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
DATA_INDEX_ROWS_SSBO;
TEXTURE_LAYER_ROWS_SSBO;

// Vertex attributes: Position + Luminance
layout(location=0) in vec3 Position;
layout(location=1) in float Luminance;

// Output to FS
layout(location=0) flat out uint fragDataIndexID;
layout(location=1) flat out uint fragTextureLayerID;
layout(location=2)      out float fragLuminance;

void main()
{
    const uint transform_id = ResolveTransformID(gl_InstanceIndex);
    const uint data_index_id = ResolveDataIndexID(gl_InstanceIndex);
    const uint texture_layer_id = ResolveTextureLayerID(gl_InstanceIndex);
    mat4 l2w_mat = l2w.mats[transform_id];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);

    fragDataIndexID = data_index_id;
    fragTextureLayerID = texture_layer_id;
    fragLuminance = Luminance;

    gl_Position = camera.vp * worldPos;
}
