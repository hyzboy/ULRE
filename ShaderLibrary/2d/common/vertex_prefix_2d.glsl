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
#include "common/instance_rows_ssbo.glsl"
L2W_INDEX_ROWS_SSBO;
#endif

#ifdef HAS_MI
#include "common/instance_rows_ssbo.glsl"
DATA_INDEX_ROWS_SSBO;
TEXTURE_LAYER_ROWS_SSBO;
#endif

// ---- Vertex inputs (location 连续排列) ----
layout(location=0) in POSITION_FORMAT Position;

#ifdef HAS_L2W
layout(location=1) in uint TransformID;
  #ifdef HAS_MI
layout(location=2) in uint DataIndexID;
layout(location=3) in uint TextureLayerID;
    #define NEXT_LOC 4
  #else
    #define NEXT_LOC 2
  #endif
#elif defined(HAS_MI)
layout(location=1) in uint DataIndexID;
layout(location=2) in uint TextureLayerID;
  #define NEXT_LOC 3
#else
  #define NEXT_LOC 1
#endif

// ---- Helper functions ----
#ifdef HAS_L2W
mat4 GetLocalToWorld() { return l2w.mats[ResolveTransformID(gl_InstanceIndex)]; }
#endif

#ifdef HAS_MI
uint GetDataIndexID2D() { return ResolveDataIndexID(gl_InstanceIndex); }
uint GetTextureLayerID2D() { return ResolveTextureLayerID(gl_InstanceIndex); }
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
