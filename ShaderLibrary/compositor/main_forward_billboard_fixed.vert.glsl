#version 450


#extension GL_EXT_scalar_block_layout : require

#include "common/ubo_camera.glsl"
#include "common/ubo_viewport.glsl"
#include "common/ssbo_transform.glsl"
struct MaterialInstance {
    uvec2 BillboardSize;
};

#define MATERIAL_INSTANCE_SSBO_SCALAR
#include "common/ssbo_material_instance.glsl"
#undef MATERIAL_INSTANCE_SSBO_SCALAR
layout(location=POSITION_LOCATION) in vec3  Position;

layout(location=0) flat out uint fragMaterialInstanceID;
layout(location=1) out vec2 fragTexCoord;

MaterialInstance GetMI() { return GetMaterialInstance(); }

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * GetTransform() * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
