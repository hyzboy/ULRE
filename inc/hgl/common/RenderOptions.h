#pragma once

#include<hgl/mtl/DescriptorKind.h>

// LocalToWorld options
// Define HGL_L2W_USE_SSBO for storage buffer, or HGL_L2W_USE_UBO for uniform buffer.
// Default: SSBO. If neither is defined, defaults to SSBO.
#if !defined(HGL_L2W_USE_SSBO) && !defined(HGL_L2W_USE_UBO)
    #define HGL_L2W_USE_SSBO
    #define TransformDescriptorKind DescriptorKind::SSBO
#else
    #define TransformDescriptorKind DescriptorKind::UBO
#endif

#if defined(HGL_L2W_USE_SSBO) && defined(HGL_L2W_USE_UBO)
#error "HGL_L2W_USE_SSBO and HGL_L2W_USE_UBO cannot both be defined"
#endif

#ifndef HGL_L2W_RING_FRAMES
#define HGL_L2W_RING_FRAMES 3
#endif

// TransformID format options
// 1: Use R32_UINT (u32)
// 0: Use R16_UINT (u16)        //这个需要storageBuffer16BitAccess支持
#ifndef HGL_TRANSFORM_ID_U32
#define HGL_TRANSFORM_ID_U32 1
#endif

// TransformID storage options
// TransformID is descriptor-backed SSBO only.
#define HGL_TRANSFORM_ID_USE_SSBO
#define TransformIDDescriptorKind DescriptorKind::SSBO

#if !defined(HGL_TRANSFORM_ID_U32) || (HGL_TRANSFORM_ID_U32==0)
    #error "Descriptor-backed TransformID currently requires HGL_TRANSFORM_ID_U32=1"
#endif

// MaterialInstance buffer options
// Define HGL_MI_USE_SSBO for storage buffer, or HGL_MI_USE_UBO for uniform buffer.
// Default: SSBO. If neither is defined, defaults to SSBO.
#if !defined(HGL_MI_USE_SSBO) && !defined(HGL_MI_USE_UBO)
    #define HGL_MI_USE_SSBO
    #define MaterialInstanceDescriptorKind DescriptorKind::SSBO
#else
    #define MaterialInstanceDescriptorKind DescriptorKind::UBO
#endif

#if defined(HGL_MI_USE_SSBO) && defined(HGL_MI_USE_UBO)
#error "HGL_MI_USE_SSBO and HGL_MI_USE_UBO cannot both be defined"
#endif

// MaterialInstanceID storage options
// Controls how per-instance MaterialInstance index is dispatched:
// - HGL_MI_ID_USE_VAB  : per-instance vertex attribute buffer (R16_UINT) [default]
// - HGL_MI_ID_USE_SSBO : descriptor-backed storage buffer (uint[])
#if !defined(HGL_MI_ID_USE_VAB) && !defined(HGL_MI_ID_USE_SSBO)
    //#define HGL_MI_ID_USE_VAB
    #define HGL_MI_ID_USE_SSBO
#endif

#if defined(HGL_MI_ID_USE_VAB) && defined(HGL_MI_ID_USE_SSBO)
    #error "Only one MaterialInstanceID storage mode can be enabled"
#endif

#if defined(HGL_MI_ID_USE_SSBO)
    #define MaterialInstanceIDDescriptorKind DescriptorKind::SSBO
#endif
