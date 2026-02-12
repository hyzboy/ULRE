#pragma once

// LocalToWorld options
// Toggle this to switch LocalToWorld between SSBO and UBO paths.
//
// 1: Use SSBO (storage buffer)
// 0: Use UBO (uniform buffer)
#ifndef HGL_L2W_USE_SSBO
#define HGL_L2W_USE_SSBO 1
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
// 1: Use SSBO (storage buffer)
// 0: Use UBO (uniform buffer)
#ifndef HGL_MI_USE_SSBO
#define HGL_MI_USE_SSBO 0
#endif
