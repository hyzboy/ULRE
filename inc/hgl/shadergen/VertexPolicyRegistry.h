#pragma once

// inc/hgl/shadergen/VertexPolicyRegistry.h
//
// VertexPolicy registry: maps a VertexTransformPolicy value to its GLSL include
// path and declares which UBOs the policy file requires.
//
// ─── CONTRACT ────────────────────────────────────────────────────────────────
//
// Every file under ShaderLibrary/vertex_policy/*.glsl MUST expose exactly one
// public function:
//
//   void ApplyVertexTransform(vec3  local_pos,
//                             out vec4 world_pos,
//                             out vec4 clip_pos);
//
//   local_pos  – object-space position, already obtained via GetPositionLocal()
//                from the selected position_provider.  The policy MUST NOT
//                call GetPositionLocal() itself; that would couple the two axes.
//
//   world_pos  – homogeneous world-space position.
//                Set world_pos.w = 1.0 when the value is meaningful (Mesh3D).
//                Set world_pos.w = 0.0 when it is undefined/not meaningful
//                (e.g. BillboardAxisLocked uses clip-space math directly).
//
//   clip_pos   – final clip-space position to assign to gl_Position.
//
// ─── UBO NEEDS ────────────────────────────────────────────────────────────────
//
// needs_camera, needs_viewport, needs_transform are populated automatically by
// VertexPolicyRegistry at first use via @sfm:require annotations in the .glsl
// file.  Do NOT set these manually; they are derived from shader source truth.
//
// ─── ADDING A NEW POLICY ──────────────────────────────────────────────────────
//
// 1. Add a value to mtl::VertexTransformPolicy (MaterialVariantRow.h).
// 2. Create ShaderLibrary/vertex_policy/<name>.glsl implementing the contract.
// 3. Add a kBuiltinPolicies entry here.
// 4. No other C++ files need changing.
//
// ─────────────────────────────────────────────────────────────────────────────

#include <hgl/mtl/MaterialVariantRow.h>
#include <string_view>

namespace hgl::graph
{

struct VertexPolicyDescriptor
{
    mtl::VertexTransformPolicy  policy;
    std::string_view            glsl_path;        ///< relative to ShaderLibrary root
    bool                        needs_camera    = false; ///< auto-filled from @sfm:require camera
    bool                        needs_viewport  = false; ///< auto-filled from @sfm:require viewport
    bool                        needs_transform = false; ///< auto-filled from @sfm:require transform_id/transform_data
};

/// Returns a pointer to the built-in VertexPolicyDescriptor for @p policy,
/// or nullptr if @p policy is unknown/not a built-in.
const VertexPolicyDescriptor *FindBuiltinVertexPolicy(mtl::VertexTransformPolicy policy) noexcept;

} // namespace hgl::graph
