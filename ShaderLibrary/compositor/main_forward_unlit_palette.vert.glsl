#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL

#include "compositor/vert_forward_ubo.glsl"
#include "common/ubo_color_palette.glsl"

layout(location=POSITION_LOCATION) in vec3  Position;
layout(location=COLOR_LOCATION) in uint  ColorIndex;

#define VARYING_STAGE_VERT
#define HAS_COLOR
#include "common/varying_interface.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    mat4 transform_mat = GetTransform();
    vec4 worldPos = transform_mat * vec4(Position, 1.0);

    fragVertexColor = unpackUnorm4x8(color_palette.color[ColorIndex]);

    gl_Position = camera.vp * worldPos;
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_UNLIT_PALETTE_VERT_GLSL
