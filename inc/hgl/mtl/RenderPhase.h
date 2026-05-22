// RenderPhase.h
// Enumerates the different rendering phases/passes in the pipeline.
// Used as a dimension for shader variant selection and pass routing.

#pragma once
#include <cstdint>

namespace hgl::graph::mtl {

enum class RenderPhase : uint8_t {
    EarlyZ = 0,                ///< Depth-only pre-pass
    ShadowCaster,              ///< Shadow map generation
    VisibilityBuffer,          ///< Visibility buffer generation
    Velocity,                  ///< Motion vector generation
    GBuffer,                   ///< Deferred GBuffer write
    ForwardOpaque,             ///< Forward opaque rendering
    ForwardMasked,             ///< Forward alpha-tested rendering
    ForwardTransparent,        ///< Forward transparent blending
    TiledForward,              ///< Tiled forward shading
    DeferredLighting,          ///< Deferred lighting pass
    PostProcess,               ///< Post-processing effects
    Tonemap,                   ///< HDR → SDR tone mapping
    UI,                        ///< UI overlay rendering
    DebugOverlay,              ///< Debug/diagnostic overlay
    COUNT
};

constexpr const char* RenderPhaseNames[] = {
    "EarlyZ",
    "ShadowCaster",
    "VisibilityBuffer",
    "Velocity",
    "GBuffer",
    "ForwardOpaque",
    "ForwardMasked",
    "ForwardTransparent",
    "TiledForward",
    "DeferredLighting",
    "PostProcess",
    "Tonemap",
    "UI",
    "DebugOverlay"
};

static_assert(sizeof(RenderPhaseNames) / sizeof(RenderPhaseNames[0]) == static_cast<size_t>(RenderPhase::COUNT),
              "RenderPhaseNames array size must match RenderPhase::COUNT");

inline const char* GetRenderPhaseName(RenderPhase phase) {
    return RenderPhaseNames[static_cast<size_t>(phase)];
}

} // namespace hgl::graph::mtl
