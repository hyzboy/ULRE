#pragma once

// LocalToWorld options
// Define HGL_L2W_USE_SSBO for storage buffer, or HGL_L2W_USE_UBO for uniform buffer.
// Default: SSBO. If neither is defined, defaults to SSBO.
#if !defined(HGL_L2W_USE_SSBO) && !defined(HGL_L2W_USE_UBO)
#define HGL_L2W_USE_SSBO
#endif

#if defined(HGL_L2W_USE_SSBO) && defined(HGL_L2W_USE_UBO)
#error "HGL_L2W_USE_SSBO and HGL_L2W_USE_UBO cannot both be defined"
#endif

#ifndef HGL_L2W_RING_FRAMES
#define HGL_L2W_RING_FRAMES 3
#endif

// TransformID format options
// 1: Use R32_UINT (u32)
// 0: Use R16_UINT (u16)
#ifndef HGL_TRANSFORM_ID_U32
#define HGL_TRANSFORM_ID_U32 0
#endif

// MaterialInstance buffer options
// Define HGL_MI_USE_SSBO for storage buffer, or HGL_MI_USE_UBO for uniform buffer.
// Default: SSBO. If neither is defined, defaults to SSBO.
#if !defined(HGL_MI_USE_SSBO) && !defined(HGL_MI_USE_UBO)
#define HGL_MI_USE_SSBO
#endif

#if defined(HGL_MI_USE_SSBO) && defined(HGL_MI_USE_UBO)
#error "HGL_MI_USE_SSBO and HGL_MI_USE_UBO cannot both be defined"
#endif
