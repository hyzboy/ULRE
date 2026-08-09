// @ulre begin
// @ulre name main_forward_surface
// @ulre kind FragmentShader
// @ulre priority 0
// @ulre uses surface_interface
// @ulre uses material_source_interface
// @ulre uses lighting_interface
// @ulre uses alpha_compositor
// @ulre end
#version 450

#ifndef MAIN_FORWARD_SURFACE_FRAG_GLSL
#define MAIN_FORWARD_SURFACE_FRAG_GLSL

#ifndef HGL_USE_SCENE_LIGHTING
#define HGL_USE_SCENE_LIGHTING 0
#endif
#ifndef HGL_USE_MATERIAL_SOURCE_PROVIDER
#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1
#endif
#ifndef HGL_USE_NTB_PROVIDER
#define HGL_USE_NTB_PROVIDER 0
#endif

#include "common/descriptor_macros.glsl"
#include "common/surface_interface.glsl"

#if HGL_USE_SCENE_LIGHTING
#include "ubo/camera_info.glsl"
#include "ubo/sky_info.glsl"
SCENE_CAMERA_UBO;
SCENE_SKY_UBO;
#include "sky/sky_atmosphere.glsl"
#include "lighting/direct_cook_torrance_pbr.glsl"
#include "lighting/indirect_simple_ambient.glsl"
#endif

#include "lighting/forward_pbr.glsl"
#if HGL_USE_MATERIAL_SOURCE_PROVIDER
#include "material/pbr_surface_source.glsl"
#endif
#if HGL_USE_NTB_PROVIDER
#include "ntb/ntb_tangent_vbo_normalmap.glsl"
#endif
#include "compositor/forward_lighting.glsl"

// ULRE_FRAGMENT_INPUT_CONTRACT
// ULRE_OUTPUT_CONTRACT

#include SURFACE_FUNCTION_FILE
#include "common/alpha_compositor.glsl"

void main()
{
// ULRE_SURFACE_INPUT_CONTRACT
    const SurfaceOutput surface =
        EvalSurface(si, materialDataIndex);
    const LightingInput lighting =
        BuildForwardLightingInput(surface, si);
    const vec4 finalColor = EvalLighting(lighting);
    WriteMaterialOutput(HGLComposeColor(finalColor));
}

#endif
