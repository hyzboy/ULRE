#pragma once

// All render data (LocalToWorld, TransformID, MaterialInstance, MaterialInstanceID)
// are descriptor-backed SSBO only. No conditional compilation branches.

#ifndef HGL_L2W_RING_FRAMES
#define HGL_L2W_RING_FRAMES 3
#endif

// Descriptor kinds used in rendering pipeline:
// - TransformDescriptorKind = DescriptorKind::SSBO
// - MaterialInstanceDescriptorKind = DescriptorKind::SSBO
// - MaterialInstanceIDDescriptorKind = DescriptorKind::SSBO
