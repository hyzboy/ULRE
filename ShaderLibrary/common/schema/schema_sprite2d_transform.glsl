#ifndef ULRE_SHADER_SCHEMA_SPRITE2D_TRANSFORM_GLSL
#define ULRE_SHADER_SCHEMA_SPRITE2D_TRANSFORM_GLSL
// Per-instance Sprite2D transform data.
// std430 layout — must match Sprite2DTransform in Sprite2DMaterialBindingSystem.cpp exactly.
// Total: 32 bytes.
struct MaterialBindingInstance {
    vec2  size;         //  8 bytes  offset  0
    vec2  pivot;        //  8 bytes  offset  8
    float rotation;     //  4 bytes  offset 16
    uint  tint_rgba8;   //  4 bytes  offset 20
    uint  flags;        //  4 bytes  offset 24
    uint  _pad0;        //  4 bytes  offset 28
};
#endif
