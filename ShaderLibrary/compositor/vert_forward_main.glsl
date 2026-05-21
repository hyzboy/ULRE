#ifndef ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL
#define ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL

// vert_forward_main.glsl -- Unified forward vertex entry point (policy-driven).
//
// Emit order enforced by CompositorAssembler::BuildForwardVertexEntry():
//   1. #include "position_provider/<file>.glsl"  → vec4 GetPosition()
//   2. conditionally: #include "common/ubo_camera.glsl"  (when needs_camera)
//      conditionally: #include "common/ssbo_transform.glsl"  (when needs_transform)
//      always:        #define MATERIAL_INSTANCE_ID_ONLY + #include "common/ssbo_material_instance.glsl"
//      conditionally: #include "common/ubo_viewport.glsl"  (when policy needs_viewport)
//   3. #include "vertex_policy/<file>.glsl"  → void ApplyVertexTransform(...)
//   4. #include "compositor/vert_forward_main.glsl"  (this file)
//
// Control defines set by CompositorAssembler:
//   HAS_POSITION / HAS_NORMAL / HAS_TANGENT / HAS_TEXCOORD
//   HAS_COLOR / HAS_LUMINANCE / HAS_DIRECTION
//   GEOMETRY_FETCH_SSBO  — read geometry from SSBO instead of vertex attribs

// --- VBO vertex inputs (skipped for PCG / SSBO providers) ---
#if !defined(GEOMETRY_FETCH_SSBO)
    #ifdef HAS_NORMAL
        layout(location=NORMAL_LOCATION) in vec3 inNormal;
    #endif

    #if defined(HAS_TANGENT) && defined(TANGENT_LOCATION)
        layout(location=TANGENT_LOCATION) in vec4 inTangent;
    #endif

    #ifdef HAS_TEXCOORD
        layout(location=TEXCOORD_LOCATION) in vec2 inUV0;
    #endif

    #ifdef HAS_COLOR
        layout(location=COLOR_LOCATION) in vec4 inColor;
    #endif

    #ifdef HAS_LUMINANCE
        layout(location=LUMINANCE_LOCATION) in float inLuminance;
    #endif
#endif

// --- Varying outputs ---
#include "common/varying_vs.glsl"

void main()
{
    fragMaterialInstanceID = GetMaterialInstanceID();

#ifdef CLIP_NDC_PROVIDER
    // ClipNDC provider: GetPosition() already returns clip-space coords; no transform needed.
    gl_Position = GetPosition();
#else
    // ── Axis 1: position provider (already included above) ─────────────────
    vec3 local = GetPosition().xyz;

    // ── Axis 2: vertex policy (already included above) ──────────────────────
    vec4 worldPos;
    vec4 clipPos;
    ApplyVertexTransform(local, worldPos, clipPos);

    // ── Attribute writers ────────────────────────────────────────────────────
    // Writes fragWorldPos / fragWorldNormal / fragWorldTangent / fragUV0 /
    // fragVertexColor / fragLuminance / fragDirection based on HAS_* defines.
    // Normal & tangent transforms require POLICY_HAS_TRANSFORM_MAT (set by mesh3d).
#include "compositor/vert_attrib_writers.glsl"

    gl_Position = clipPos;
#endif
}

#endif // ULRE_COMPOSITOR_VERT_FORWARD_MAIN_GLSL
