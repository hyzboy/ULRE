#pragma once

#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph::mtl
{
    constexpr uint32 MaterialShaderStageMask =
        uint32(ShaderStage::Vertex) | uint32(ShaderStage::Fragment);

    constexpr bool HasUnsupportedMaterialShaderStageBits(const uint32 stage_bits) noexcept
    {
        return (stage_bits & ~MaterialShaderStageMask) != 0;
    }

    constexpr bool HasRequiredMaterialShaderStages(const uint32 stage_bits) noexcept
    {
        return (stage_bits & uint32(ShaderStage::Vertex)) != 0
            && (stage_bits & uint32(ShaderStage::Fragment)) != 0;
    }

    constexpr bool IsSupportedMaterialShaderStageMask(const uint32 stage_bits) noexcept
    {
        return stage_bits != 0
            && !HasUnsupportedMaterialShaderStageBits(stage_bits)
            && HasRequiredMaterialShaderStages(stage_bits);
    }

    constexpr uint32 NormalizeMaterialShaderStageMask(const uint32 stage_bits) noexcept
    {
        return stage_bits & MaterialShaderStageMask;
    }
}
