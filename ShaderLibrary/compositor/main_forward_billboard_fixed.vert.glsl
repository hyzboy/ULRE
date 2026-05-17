#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL

// Billboard axis-locked (fixed screen-pixel size).
// Mesh:   unit quad, Position2D in [-0.5, 0.5]^2, with TexCoord.
// Scale:  Transform.scale.xy = screen-pixel size.
//         length(M[0].xyz) = pixel width,  length(M[1].xyz) = pixel height.
//         Transform rotation is intentionally ignored.
#define POSITION_KIND 1
#include "common/vertex_input_position.glsl"

#include "common/ubo_camera.glsl"
#include "common/ubo_viewport.glsl"
#include "common/ssbo_transform.glsl"

#define VARYING_STAGE_VERT
#define HAS_TEXCOORD
#include "common/varying_interface.glsl"

layout(location=3) in vec2 in_TexCoord;

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 M = GetTransform();
    vec2 local_offset = GetPositionLocal();          // in [-0.5, 0.5]

    // Center: project translation only (ignore scale & rotation).
    vec4 center_clip = camera.vp * vec4(M[3].xyz, 1.0);
    vec2 center_ndc  = center_clip.xy / center_clip.w;

    // Extract per-axis pixel size from Transform's column lengths.
    float px = length(M[0].xyz);                     // pixel width
    float py = length(M[1].xyz);                     // pixel height
    vec2 psize_ndc = vec2(px, py) * 2.0 / vec2(viewport.canvas_resolution);

    vec2 ndc = center_ndc + local_offset * psize_ndc;

    fragUV0 = in_TexCoord;

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
