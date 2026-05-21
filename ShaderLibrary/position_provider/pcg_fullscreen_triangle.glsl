// position_provider/pcg_fullscreen_triangle.glsl
//
// Position source: procedural – computes NDC positions from gl_VertexIndex.
// Three vertices cover the entire screen (including clip-space overflow) so
// the rasteriser fills every fragment exactly once.
//
// GetPosition() returns coordinates in clip/NDC space (output_space: clip_ndc).
// The compositor MUST emit:  gl_Position = GetPosition();
// and must NOT apply any MVP transform.
//
// @sfm version: 1
// @sfm kind: pcg
// @sfm output_space: clip_ndc
// @sfm consumes_vab: false
// @sfm allow_dim_override: false

#ifndef ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL
#define ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL

vec4 GetPosition()
{
    // Vertices at NDC (−1,−1), (3,−1), (−1,3) cover the entire clip quad.
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    return vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

#endif // ULRE_POS_PCG_FULLSCREEN_TRIANGLE_GLSL
