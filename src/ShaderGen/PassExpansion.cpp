#include <hgl/mtl/PassExpansion.h>

namespace hgl::graph::mtl
{

std::span<const PassType> GetPassTypesForBlendMode(RenderAlphaMode blend) noexcept
{
    using PT = PassType;

    // Mirror the pass tables from CompositorAssembler.cpp
    static constexpr PassType kOpaque[]          = { PT::ForwardOpaque, PT::ShadowOpaque, PT::EarlyZSolid };
    static constexpr PassType kMasked[]          = { PT::ForwardMasked, PT::ShadowMasked, PT::EarlyZMasked };
    static constexpr PassType kTransparent[]     = { PT::ForwardTransparent };
    static constexpr PassType kDither[]          = { PT::ForwardDither, PT::ShadowOpaque };
    static constexpr PassType kAlphaToCoverage[] = { PT::ForwardA2C, PT::ShadowMasked };

    switch (blend)
    {
    case RenderAlphaMode::Opaque:          return kOpaque;
    case RenderAlphaMode::Masked:          return kMasked;
    case RenderAlphaMode::Transparent:     return kTransparent;
    case RenderAlphaMode::Dither:          return kDither;
    case RenderAlphaMode::AlphaToCoverage: return kAlphaToCoverage;
    default:                               return kOpaque;
    }
}

} // namespace hgl::graph::mtl
