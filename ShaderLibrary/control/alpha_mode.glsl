// control/alpha_mode.glsl — Alpha mode numeric constants
//
// Mirrors the C++ RenderAlphaMode enum values injected as SURFACE_TYPE.
// Include only if a surface shader needs to branch on the alpha mode at compile time.

#ifndef ULRE_CONTROL_ALPHA_MODE_GLSL
#define ULRE_CONTROL_ALPHA_MODE_GLSL

// Values must stay in sync with C++ enum RenderAlphaMode in VKGraphicsPipelinePreset.h
#define ALPHA_MODE_VALUE_OPAQUE       0
#define ALPHA_MODE_VALUE_MASKED       1
#define ALPHA_MODE_VALUE_TRANSPARENT  2
#define ALPHA_MODE_VALUE_DITHER       3
#define ALPHA_MODE_VALUE_A2C          4

#endif // ULRE_CONTROL_ALPHA_MODE_GLSL
