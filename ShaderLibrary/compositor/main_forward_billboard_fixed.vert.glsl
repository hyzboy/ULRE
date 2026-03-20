#version 450


#extension GL_EXT_scalar_block_layout : require

#include "common/descriptor_macros.glsl"
#include "common/scene_ubo.glsl"
SCENE_CAMERA_UBO;
SCENE_VIEWPORT_UBO;

#include "common/l2w_ssbo.glsl"
struct MaterialInstance {
    uvec2 BillboardSize;
};

#define MATERIAL_INSTANCE_SSBO_SCALAR
#include "common/material_instance_ssbo.glsl"
#undef MATERIAL_INSTANCE_SSBO_SCALAR
layout(location=POSITION_LOCATION) in vec3  Position;

layout(location=0) out vec2 fragTexCoord;

MaterialInstance GetMI() { return GetMaterialInstance(); }

void main()
{
    MaterialInstance mi = GetMI();

    vec2 psize = vec2(mi.BillboardSize) / vec2(viewport.canvas_resolution);
    vec4 center_clip = camera.vp * GetTransform() * vec4(0.0, 0.0, 0.0, 1.0);
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + Position.xy * psize;

    fragTexCoord = vec2(Position.x + 0.5, Position.y + 0.5);

    gl_Position = vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
