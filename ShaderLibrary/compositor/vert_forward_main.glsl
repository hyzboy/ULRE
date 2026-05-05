#ifndef ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL
#define ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL

// vert_forward_main.glsl -- Unified forward vertex entry point.
//
// Injected by C++ CompositorAssembler after:
//   common/vertex_input_position.glsl  (inPosition decl + GetPositionLocal())
//   compositor/vert_forward_ubo.glsl   (camera, transform, MI id)
//   common/varying_interface.glsl (VARYING_STAGE_VERT must be defined first)
//
// Control defines (set by CompositorAssembler):
//   HAS_POSITION / HAS_NORMAL / HAS_TANGENT / HAS_TEXCOORD
//   HAS_COLOR / HAS_LUMINANCE / HAS_DIRECTION
//   POSITION_KIND       -- 0=None, 1=Vec2, 2=Vec3 (handled by vertex_input_position.glsl)
//   GEOMETRY_FETCH_SSBO -- read geometry from SSBO instead of vertex attribs

// --- Vertex inputs ---
#if GEOMETRY_FETCH_SSBO
    #include "common/vertex_fetch_ssbo.glsl"
#else
    // Legacy compatibility declarations for non-SSBO materials.
    #include "common/vertex_input_compat.glsl"

#endif

// --- Varying interface ---
#include "common/varying_vs.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();

#include "compositor/vert_input_resolve.glsl"

    gl_Position = camera.vp * worldPos;
}

#endif // ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL
