#include <hgl/shadergen/ColorSourceBridge.h>

namespace hgl::graph
{

std::vector<ColorSource> LegacyTexturesToColorSources(
    const std::vector<LegacyTextureSlotDesc> &legacy)
{
    std::vector<ColorSource> result;
    result.reserve(legacy.size());

    for (const auto &desc : legacy)
    {
        const auto fmt = ChannelHintToOutputFormat(desc.channel);

        switch (desc.source_mode)
        {
        case mtl::TextureSourceMode::None:
            // 跳过未配置的槽
            break;

        case mtl::TextureSourceMode::Simple:
        case mtl::TextureSourceMode::Atlas: // Atlas 底层与 2D 一致，UV 偏移由 surface shader 处理
            result.push_back(ColorSource::MakeSampler2D(desc.slot, fmt));
            break;

        case mtl::TextureSourceMode::Array:
            result.push_back(ColorSource::MakeSampler2DArray(desc.slot, fmt));
            break;

        case mtl::TextureSourceMode::PCG:
        {
            // PCG 槽：生成一个占位 ColorSource，kind = None，等待调用方填充 user 载荷
            ColorSource cs;
            cs.slot      = desc.slot;
            cs.kind      = ColorSourceKind::UserPCG;
            cs.signature = ColorSourceSignature::UV2D; // 默认签名，用户可覆写
            cs.builtin.output_format = fmt;
            result.push_back(std::move(cs));
            break;
        }

        default:
            break;
        }
    }

    return result;
}

} // namespace hgl::graph
