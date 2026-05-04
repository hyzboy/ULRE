#ifndef ULRE_2D_GET_POSITION_2D_GLSL
#define ULRE_2D_GET_POSITION_2D_GLSL

/// get_position_2d.glsl — 2D 顶点位置计算
///
/// 由 Build2DVertexPreamble 注入以下 #define 控制变体：
///   COORD_ORTHO         — 正交投影
///   COORD_ZERO_TO_ONE   — [0,1] 坐标映射到 [-1,1]
///   HAS_LOCAL_TO_WORLD  — 需要 LocalToWorld 变换
///   POSITION_TYPE       — 顶点输入类型（vec2 / ivec2 / …）
///   POSITION_LOCATION   — 顶点输入 location（由 layout defines 注入）

#ifdef COORD_ORTHO
#include "common/ubo_viewport.glsl"
#endif

#ifdef HAS_LOCAL_TO_WORLD
#include "common/ssbo_transform.glsl"
#endif

#ifdef POSITION_SSBO_BINDING
#ifndef POSITION_SSBO_SET
#define POSITION_SSBO_SET VERTEXSTREAMS_SET
#endif
#if defined(POSITION_SSBO_IS_VEC2)
#include "position_provider/ssbo_packed_vec2.glsl"
#else
#include "position_provider/ssbo_packed.glsl"
#endif
#else
layout(location=POSITION_LOCATION) in POSITION_TYPE Position;
#endif

vec2 GetPosition2DInput()
{
#ifdef POSITION_SSBO_BINDING
    return vec2(GetPositionLocal());
#else
    return vec2(Position);
#endif
}

vec4 GetPosition2D()
{
#if defined(COORD_ORTHO) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * viewport.ortho_matrix * vec4(GetPosition2DInput(), 0, 1);
#elif defined(COORD_ORTHO)
    return viewport.ortho_matrix * vec4(GetPosition2DInput(), 0, 1);
#elif defined(COORD_ZERO_TO_ONE) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(GetPosition2DInput() * 2.0 - 1.0, 0, 1);
#elif defined(COORD_ZERO_TO_ONE)
    return vec4(GetPosition2DInput() * 2.0 - 1.0, 0, 1);
#elif defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(GetPosition2DInput(), 0, 1);
#else
    return vec4(GetPosition2DInput(), 0, 1);
#endif
}

#endif // ULRE_2D_GET_POSITION_2D_GLSL
