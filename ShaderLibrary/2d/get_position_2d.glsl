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

layout(location=POSITION_LOCATION) in POSITION_TYPE Position;

vec4 GetPosition2D()
{
#if defined(COORD_ORTHO) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ORTHO)
    return viewport.ortho_matrix * vec4(vec2(Position), 0, 1);
#elif defined(COORD_ZERO_TO_ONE) && defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(COORD_ZERO_TO_ONE)
    return vec4(vec2(Position) * 2.0 - 1.0, 0, 1);
#elif defined(HAS_LOCAL_TO_WORLD)
    return GetTransform() * vec4(vec2(Position), 0, 1);
#else
    return vec4(vec2(Position), 0, 1);
#endif
}
