#ifndef ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
#define ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL

// Billboard fixed-size: always uses a vec3 position attribute (xy = quad offset, z = depth).
#define POSITION_KIND 2
#include "common/vertex_input_position.glsl"

#include "common/ubo_camera.glsl"
#include "common/ubo_viewport.glsl"
#include "common/ssbo_transform.glsl"

#include "common/ssbo_material_instance.glsl"

#define VARYING_STAGE_VERT
#define HAS_BILLBOARD_TEXCOORD
#include "common/varying_interface.glsl"

MaterialBindingInstance GetMI() { return GetMaterialBindingInstance(); }

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    MaterialBindingInstance mi = GetMI();

    vec3 pos3 = GetPositionLocal();
    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * GetTransform() * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + pos3.xy * psize;

    fragTexCoord = vec2(pos3.x + 0.5, pos3.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}

#endif // ULRE_COMPOSITOR_MAIN_FORWARD_BILLBOARD_FIXED_VERT_GLSL
