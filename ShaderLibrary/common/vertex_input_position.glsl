#ifndef ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL
#define ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL

// common/vertex_input_position.glsl -- Position vertex input declaration + GetPositionLocal().
//
// Injected by C++ CompositorAssembler (or self-defined in standalone .vert.glsl files).
//
// Prerequisites:
//   POSITION_LOCATION -- vertex input location for the position attribute
//   POSITION_KIND     -- integer: 0 = None (no VBO / procedural)
//                                 1 = Vec2 (2D position, padded z=0)
//                                 2 = Vec3 (3D position)
//
// Provides:
//   [layout in declaration]  inPosition  (only for POSITION_KIND 1 or 2)
//   vec3 GetPositionLocal()  -- returns object-space position as vec3

#if POSITION_KIND == 2
    layout(location=POSITION_LOCATION) in vec3 inPosition;
    vec3 GetPositionLocal() { return inPosition; }
#elif POSITION_KIND == 1
    layout(location=POSITION_LOCATION) in vec2 inPosition;
    vec3 GetPositionLocal() { return vec3(inPosition, 0.0); }
#elif POSITION_KIND == 0
    // No vertex buffer position attribute; caller provides position via other means.
    #if defined(POSITION_SSBO_BINDING)
        // Position is supplied by position_provider/ssbo_packed.glsl.
        // Do not emit a fallback GetPositionLocal() here to avoid duplicate
        // function bodies when vertex pulling is enabled.
    #else
        #ifndef ULRE_POSITION_PROVIDED_BY_USER
            vec3 GetPositionLocal() { return vec3(0.0); }
        #endif
    #endif
#else
    #error "POSITION_KIND must be defined as 0 (None), 1 (Vec2), or 2 (Vec3)"
#endif

#endif // ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL
