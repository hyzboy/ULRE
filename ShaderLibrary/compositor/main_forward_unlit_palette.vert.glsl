// @sfm:require  UBO camera
// @sfm:require  SSBO transform_id
// @sfm:require  SSBO transform_data
#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL

// Unlit palette VS: always uses a vec3 position attribute.
#define POSITION_KIND 2
#include "common/vertex_input_position.glsl"

#include "compositor/vert_forward_ubo.glsl"
#include "common/ubo_color_palette.glsl"

layout(location=COLOR_LOCATION) in uint  ColorIndex;

#define VARYING_STAGE_VERT
#define HAS_COLOR
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 transform_mat = GetTransform();
    vec4 worldPos = transform_mat * vec4(GetPositionLocal(), 1.0);

    fragVertexColor = unpackUnorm4x8(color_palette.color[ColorIndex]);

    gl_Position = camera.vp * worldPos;
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL
