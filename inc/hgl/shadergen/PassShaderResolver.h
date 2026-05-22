// PassShaderResolver.h
// Resolves fixed per-pass shaders for EarlyZ / ShadowCaster / Velocity / VisibilityBuffer.
//
// DESIGN:
//   These passes do NOT consult MaterialPresetTable or the Matcher.
//   Each pass has a known fixed VS+FS (or VS-only for depth) shader that depends only on:
//     - vertex_transform_policy  (how vertices are transformed)
//     - position_provider        (where position data comes from)
//     - alpha_config             (optional: Masked / AlphaToCoverage need alpha test in FS)
//
//   The result is a PassShaderOverride that can be placed in MatchedShaderSet::pass_override.
//
// USAGE (caller decides which resolver to use based on current RenderPhase):
//
//   if (IsManagedByPassResolver(phase)) {
//       auto override = PassShaderResolver::Resolve(phase, vtx_policy, pos_id, alpha_cfg);
//       matched_set.pass_override = override;
//   } else {
//       matched_set = Matcher::Resolve(...);
//   }

#pragma once

#include <hgl/mtl/RenderPhase.h>
#include <hgl/common/PositionProvider.h>
#include <hgl/shadergen/MatchedShaderSet.h>   // PassShaderOverride, AlphaOverlay
#include <optional>

namespace hgl::graph {

/// Returns true for phases that bypass Matcher and are handled by PassShaderResolver.
inline bool IsManagedByPassResolver(hgl::graph::mtl::RenderPhase phase) {
    switch (phase) {
        case hgl::graph::mtl::RenderPhase::EarlyZ:
        case hgl::graph::mtl::RenderPhase::ShadowCaster:
        case hgl::graph::mtl::RenderPhase::Velocity:
        case hgl::graph::mtl::RenderPhase::VisibilityBuffer:
            return true;
        default:
            return false;
    }
}

class PassShaderResolver {
public:
    /// Resolve the fixed VS+FS override for a pass-managed phase.
    ///
    /// @param phase           Must be one of EarlyZ / ShadowCaster / Velocity / VisibilityBuffer.
    /// @param vtx_policy      Vertex transform policy id (opaque uint32 matching recipe field).
    /// @param pos_id          Position provider id.
    /// @param alpha_overlay   Optional; when Masked/AlphaToCoverage the FS must discard.
    /// @return                PassShaderOverride with vs_path (and optionally fs_path).
    static PassShaderOverride Resolve(
        hgl::graph::mtl::RenderPhase        phase,
        uint32_t                            vtx_policy,
        PositionProviderId                  pos_id,
        const std::optional<AlphaOverlay>&  alpha_overlay = std::nullopt);

private:
    /// ShaderLibrary-relative path to the shared depth-pass VS.
    static const char* GetDepthVsPath(uint32_t vtx_policy, PositionProviderId pos_id);

    /// ShaderLibrary-relative path to the shadow FS (may be empty = depth-only).
    static const char* GetShadowFsPath(const std::optional<AlphaOverlay>& alpha_overlay);

    /// ShaderLibrary-relative path to the velocity VS.
    static const char* GetVelocityVsPath(uint32_t vtx_policy, PositionProviderId pos_id);

    /// ShaderLibrary-relative path to the visibility-buffer VS.
    static const char* GetVisibilityVsPath(uint32_t vtx_policy, PositionProviderId pos_id);
};

} // namespace hgl::graph
