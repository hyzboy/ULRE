#ifndef ULRE_GET_NORM_TANGENT_GLSL
#define ULRE_GET_NORM_TANGENT_GLSL

// Unified API used by surface shaders:
//   - ULRE_GetGeometryNormal(si)
//   - ULRE_GetNormalFromTS(si, normal_ts)
//
// Implementation is selected by compile-time feature defines so surface code
// stays identical regardless of tangent availability.

#ifdef HAS_WORLD_TANGENT
#include "common/get_norm_tangent_with_tangent.glsl"
#else
#include "common/get_norm_tangent_without_tangent.glsl"
#endif

#endif
