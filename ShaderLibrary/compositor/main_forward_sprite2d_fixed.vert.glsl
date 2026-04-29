#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_FIXED_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_FIXED_VERT_GLSL

// Sprite2DAxisLocked — fixed pixel size, screen-aligned billboard.
// Position is vec2 (unit square [-0.5, 0.5] in both axes).
// mi.BillboardSize drives the screen-space pixel dimensions.

#include "common/ubo_camera.glsl"
#include "common/ubo_viewport.glsl"
#include "common/ssbo_transform.glsl"
#include "common/ssbo_material_instance.glsl"

layout(location=POSITION_LOCATION) in vec2 Position;

#define VARYING_STAGE_VERT
#define HAS_BILLBOARD_TEXCOORD
#include "common/varying_interface.glsl"

MaterialBindingInstance GetMI() { return GetMaterialBindingInstance(); }

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    MaterialBindingInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * GetTransform() * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_SPRITE2D_FIXED_VERT_GLSL
