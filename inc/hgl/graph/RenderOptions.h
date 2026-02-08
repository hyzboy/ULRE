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
