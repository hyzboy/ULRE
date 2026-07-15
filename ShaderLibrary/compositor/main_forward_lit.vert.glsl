#version 450

// === Compositor Template: Forward Lit VS ===
// Lit 材质共用顶点模板 — BasicLit, PBRColor3D, TextureBlinnPhong
//
// Descriptor binding 约定（Resort() 按字母序分配）：
//   Scene     set=0 : camera=0, sky=1, viewport=2
//   Transform set=1 : l2w=0
//   Material  set=2 : (varies per surface function)

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

// Vertex attributes: Position + TexCoord + Normal + TransformID + DataIndexID + TextureLayerID
layout(location=0) in vec3 Position;
layout(location=1) in vec2 TexCoord;
layout(location=2) in vec3 Normal;
layout(location=3) in uint TransformID;
layout(location=4) in uint DataIndexID;
layout(location=5) in uint TextureLayerID;

// Outputs to FS
layout(location=0) flat out uint fragDataIndexID;
layout(location=1) flat out uint fragTextureLayerID;
layout(location=2) out vec3 fragWorldPos;
layout(location=3) out vec3 fragWorldNormal;
layout(location=4) out vec2 fragUV0;

void main()
{
    const uint transform_id = ResolveTransformID(gl_InstanceIndex);
    const uint data_index_id = ResolveDataIndexID(gl_InstanceIndex);
    const uint texture_layer_id = ResolveTextureLayerID(gl_InstanceIndex);
    mat4 l2w_mat = l2w.mats[transform_id];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);

    fragDataIndexID = data_index_id;
    fragTextureLayerID = texture_layer_id;
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;
    fragUV0        = TexCoord;

    gl_Position = camera.vp * worldPos;
}
