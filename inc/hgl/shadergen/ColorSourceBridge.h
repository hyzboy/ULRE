#pragma once

/// ColorSourceBridge.h
///
/// 旧 TextureSlotConfig / TextureSourceMode 向 ColorSource 的迁移适配器。
/// 提供 LegacyTexturesToColorSources 用于在不改动调用方的情况下复用新管线。

#include <hgl/shadergen/ColorSource.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <vector>

namespace hgl::graph
{

/// 将旧 TextureChannelHint 映射到 ColorSourceOutputFormat
inline ColorSourceOutputFormat ChannelHintToOutputFormat(TextureChannelHint hint) noexcept
{
    return hint == TextureChannelHint::Grayscale
        ? ColorSourceOutputFormat::Grayscale_R
        : ColorSourceOutputFormat::RGBA;
}

/// 代表一条旧纹理槽配置（与 MaterialRecipe::TextureSlotConfig 字段对应）
struct LegacyTextureSlotDesc
{
    mtl::SamplerSlot       slot        = mtl::SamplerSlot::BaseColor;
    mtl::TextureSourceMode source_mode = mtl::TextureSourceMode::None;
    TextureChannelHint     channel     = TextureChannelHint::RGBA;
};

/// 将旧式纹理配置列表转换为 ColorSource 列表。
/// - TextureSourceMode::None  -> 跳过
/// - TextureSourceMode::Simple -> ColorSource::MakeSampler2D
/// - TextureSourceMode::Array  -> ColorSource::MakeSampler2DArray
/// - TextureSourceMode::Atlas  -> MakeSampler2D（Atlas 通过 UV 偏移处理，底层同 2D）
/// - TextureSourceMode::PCG    -> ColorSourceKind::None（占位，等待用户填 user 载荷）
std::vector<ColorSource> LegacyTexturesToColorSources(
    const std::vector<LegacyTextureSlotDesc> &legacy);

} // namespace hgl::graph
