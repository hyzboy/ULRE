// vertex_prefix_2d.glsl — 2D 材质顶点着色器共用前缀
//
// C++ 预定义宏（通过 Build2DPreamble 生成）:
//   POSITION_FORMAT              — 顶点位置类型 (vec2/vec4/ivec2)
//   COORD_NDC / COORD_ZEROTOONE / COORD_ORTHO — 坐标系
//   HAS_L2W                      — (可选) 启用 LocalToWorld
//   HAS_MI                       — (可选) 启用 MaterialInstance
//   SCENE_SET / VIEWPORT_BINDING — (Ortho 时) 视口 UBO
//   L2W_SET   / L2W_BINDING      — (L2W 时) 变换 SSBO

#ifndef VERTEX_PREFIX_2D_GLSL
#define VERTEX_PREFIX_2D_GLSL

// ---- Viewport UBO (Ortho only) ----
#ifdef COORD_ORTHO
#include "common/scene_ubo.glsl"
SCENE_VIEWPORT_UBO;
#endif

// ---- L2W SSBO ----
#ifdef HAS_L2W
#include "common/l2w_ssbo.glsl"
L2W_SSBO;

#include "common/transform_id_buffer.glsl"
TRANSFORM_ID_BUFFER;
#endif

#ifdef HAS_MI
#include "common/material_instance_id_buffer.glsl"
MATERIAL_INSTANCE_ID_BUFFER;
#define GET_MATERIAL_INSTANCE_ID() FetchMaterialInstanceID()
#endif

// ---- Vertex inputs (locations provided by ShaderLayoutDefineEmitter) ----
layout(location=POSITION_LOCATION) in POSITION_FORMAT Position;

// ---- Helper functions ----
#ifdef HAS_L2W
mat4 GetLocalToWorld()
{
  return l2w.mats[FetchTransformID()];
}
#endif

vec4 GetPosition2D()
{
#if defined(COORD_ORTHO) && defined(HAS_L2W)
    return GetLocalToWorld() * viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ORTHO)
    return viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ZEROTOONE) && defined(HAS_L2W)
    return GetLocalToWorld() * vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(COORD_ZEROTOONE)
    return vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(HAS_L2W)
    return GetLocalToWorld() * vec4(vec2(Position), 0, 1);
#else
    return vec4(vec2(Position), 0, 1);
#endif
}

#endif // VERTEX_PREFIX_2D_GLSL
