#ifndef ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL
#define ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL

// position_provider/pcg_fullscreen_triangle.glsl
// @sfm:no-require
//
// Position source: procedural – computes NDC positions from gl_VertexIndex.
// Three vertices cover the entire screen (including clip-space overflow) so
// the rasteriser fills every fragment exactly once.
//
// IMPORTANT: GetPositionLocal() returns coordinates already in NDC space,
// NOT object/local space.  The main vertex shader template must NOT multiply
// the result by any MVP matrix.  Use a dedicated PCG main template that emits:
//
//   gl_Position = vec4(GetPositionLocal(), 1.0);
//
// MANIFEST: {
//   "vab_count": 0,
//   "position_space": "ndc",
//   "ssbo": [], "ubo": [], "samplers": []
// }

vec3 GetPositionLocal()
{
    // Vertices at NDC (−1,−1), (3,−1), (−1,3) cover the entire clip quad.
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    return vec3(p * 2.0 - 1.0, 0.0);
}

#endif // ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL
