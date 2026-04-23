// control/sampler_slots.glsl — Sampler slot name ↔ C++ SamplerSlot enum alignment
//
// This file is DOCUMENTATION ONLY — no executable GLSL.
// It exists to keep the slot names in one place and in sync with the C++ SamplerSlot enum.
//
// C++ enum: hgl::graph::mtl::SamplerSlot  (inc/hgl/mtl/SamplerSlot.h)
//
// Slot name   | GLSL sampler symbol   | HAS_SAMPLER_X define       | C++ enum value
// ---------------------------------------------------------------------------
// BaseColor   | Sampler_BaseColor     | HAS_SAMPLER_BASECOLOR      | SamplerSlot::BaseColor
// Normal      | Sampler_Normal        | HAS_SAMPLER_NORMAL         | SamplerSlot::Normal
// Tangent     | Sampler_Tangent       | HAS_SAMPLER_TANGENT        | SamplerSlot::Tangent
// Metallic    | Sampler_Metallic      | HAS_SAMPLER_METALLIC       | SamplerSlot::Metallic
// Roughness   | Sampler_Roughness     | HAS_SAMPLER_ROUGHNESS      | SamplerSlot::Roughness
// Height      | Sampler_Height        | HAS_SAMPLER_HEIGHT         | SamplerSlot::Height
// Opacity     | Sampler_Opacity       | HAS_SAMPLER_OPACITY        | SamplerSlot::Opacity
// Text        | Sampler_Text          | HAS_SAMPLER_TEXT           | SamplerSlot::Text
//
// Getter function: vec4 GetSampler<SlotName>(vec2 uv)
//   Generated inline by SamplerGLSLEmitter.cpp for each active slot.
//   Non-array:  wraps texture(Sampler_X, uv)
//   Array:      wraps texture(Sampler_X, vec3(uv, float(_tex_layer_X)))

#ifndef ULRE_CONTROL_SAMPLER_SLOTS_GLSL
#define ULRE_CONTROL_SAMPLER_SLOTS_GLSL
// (no executable code)
#endif // ULRE_CONTROL_SAMPLER_SLOTS_GLSL
