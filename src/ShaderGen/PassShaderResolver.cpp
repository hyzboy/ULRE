// PassShaderResolver.cpp
// Stub implementation — returns placeholder paths that will be filled in once
// the ShaderLibrary pass shaders are authored.

#include <hgl/shadergen/PassShaderResolver.h>
#include <cassert>

namespace hgl::graph {

// ---------------------------------------------------------------------------
// Internal path helpers (stubs — real paths depend on ShaderLibrary layout)
// ---------------------------------------------------------------------------

const char* PassShaderResolver::GetDepthVsPath(uint32_t /*vtx_policy*/,
                                               PositionProviderId /*pos_id*/)
{
    // TODO: select depth VS based on vtx_policy / pos_id
    return "pass/depth_only.vert.glsl";
}

const char* PassShaderResolver::GetShadowFsPath(
    const std::optional<AlphaOverlay>& alpha_overlay)
{
    if (alpha_overlay.has_value()) {
        const AlphaOverlay::Mode m = alpha_overlay->mode;
        if (m == AlphaOverlay::Mode::Masked || m == AlphaOverlay::Mode::AlphaToCoverage)
            return "pass/shadow_masked.frag.glsl";
    }
    return "";  // depth-only; no FS needed
}

const char* PassShaderResolver::GetVelocityVsPath(uint32_t /*vtx_policy*/,
                                                  PositionProviderId /*pos_id*/)
{
    // TODO: select velocity VS based on vtx_policy / pos_id
    return "pass/velocity.vert.glsl";
}

const char* PassShaderResolver::GetVisibilityVsPath(uint32_t /*vtx_policy*/,
                                                    PositionProviderId /*pos_id*/)
{
    // TODO: select visibility-buffer VS based on vtx_policy / pos_id
    return "pass/visibility_buffer.vert.glsl";
}

// ---------------------------------------------------------------------------
// Resolve
// ---------------------------------------------------------------------------
PassShaderOverride PassShaderResolver::Resolve(
    hgl::mtl::RenderPhase               phase,
    uint32_t                            vtx_policy,
    PositionProviderId                  pos_id,
    const std::optional<AlphaOverlay>&  alpha_overlay)
{
    assert(IsManagedByPassResolver(phase) &&
           "PassShaderResolver::Resolve called for non-pass phase");

    PassShaderOverride result;

    switch (phase) {
        case hgl::mtl::RenderPhase::EarlyZ:
            result.vs_path = GetDepthVsPath(vtx_policy, pos_id);
            result.fs_path = GetShadowFsPath(alpha_overlay); // Masked EarlyZ needs FS discard
            break;

        case hgl::mtl::RenderPhase::ShadowCaster:
            result.vs_path = GetDepthVsPath(vtx_policy, pos_id);
            result.fs_path = GetShadowFsPath(alpha_overlay);
            break;

        case hgl::mtl::RenderPhase::Velocity:
            result.vs_path = GetVelocityVsPath(vtx_policy, pos_id);
            result.fs_path = "pass/velocity.frag.glsl";
            break;

        case hgl::mtl::RenderPhase::VisibilityBuffer:
            result.vs_path = GetVisibilityVsPath(vtx_policy, pos_id);
            result.fs_path = "pass/visibility_buffer.frag.glsl";
            break;

        default:
            break;
    }

    return result;
}

} // namespace hgl::graph
