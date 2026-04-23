#pragma once
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <span>

namespace hgl::graph::mtl
{
    /// Returns the set of PassTypes to generate for a given blend mode.
    /// The returned span points to static storage; lifetime is indefinite.
    std::span<const PassType> GetPassTypesForBlendMode(RenderAlphaMode blend) noexcept;

} // namespace hgl::graph::mtl
