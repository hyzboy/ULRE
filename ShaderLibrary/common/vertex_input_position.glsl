#ifndef ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL
#define ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL

// common/vertex_input_position.glsl -- Position vertex input declaration + GetPosition().
//
// Injected by C++ CompositorAssembler (or self-defined in standalone .vert.glsl files).
//
// Prerequisites:
//   POSITION_LOCATION -- vertex input location for the position attribute
//   POSITION_KIND     -- integer: 0 = None (no VBO / procedural)
//                                 1 = Vec2 (2D position, padded z=0 w=1)
//                                 2 = Vec3 (3D position, padded w=1)
//
// Provides:
//   [layout in declaration]  inPosition  (only for POSITION_KIND 1 or 2)
//   vec4 GetPosition()       -- returns object-space position as vec4 (w=1)

#ifndef POSITION_LOCATION
    #define POSITION_LOCATION 0
#endif

#if POSITION_KIND == 2
    layout(location=POSITION_LOCATION) in vec3 inPosition;
    vec4 GetPosition() { return vec4(inPosition, 1.0); }
#elif POSITION_KIND == 1
    layout(location=POSITION_LOCATION) in vec2 inPosition;
    vec4 GetPosition() { return vec4(inPosition, 0.0, 1.0); }
#elif POSITION_KIND == 0
    // No vertex buffer position attribute; caller provides position via other means.
    #ifndef ULRE_POSITION_PROVIDED_BY_USER
        vec4 GetPosition() { return vec4(0.0, 0.0, 0.0, 1.0); }
    #endif
#else
    #error "POSITION_KIND must be defined as 0 (None), 1 (Vec2), or 2 (Vec3)"
#endif

#endif // ULRE_COMMON_VERTEX_INPUT_POSITION_GLSL
