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

#include "common/transform_id_buffer.glsl"
TRANSFORM_ID_BUFFER;
#define GET_TRANSFORM_ID() FetchTransformID()

#include "common/material_instance_id_buffer.glsl"
MATERIAL_INSTANCE_ID_BUFFER;
#define GET_MATERIAL_INSTANCE_ID() FetchMaterialInstanceID()

// Vertex attributes: Position + TexCoord + Normal + TransformID + MaterialInstanceID
layout(location=POSITION_LOCATION) in vec3 Position;
layout(location=TEXCOORD_LOCATION) in vec2 TexCoord;
layout(location=NORMAL_LOCATION) in vec3 Normal;

// Outputs to FS
layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec3 fragWorldPos;
layout(location=2) out vec3 fragWorldNormal;
layout(location=3) out vec2 fragUV0;

void main()
{
    mat4 l2w_mat = l2w.mats[GET_TRANSFORM_ID()];
    vec4 worldPos = l2w_mat * vec4(Position, 1.0);
    vec3 worldNormal = normalize(mat3(l2w_mat) * Normal);

    fragMaterialInstanceID = GET_MATERIAL_INSTANCE_ID();
    fragWorldPos   = worldPos.xyz;
    fragWorldNormal = worldNormal;
    fragUV0        = TexCoord;

    gl_Position = camera.vp * worldPos;
}
