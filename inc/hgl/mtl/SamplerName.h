#pragma once

#include <cstdint>
#include <cstring>

namespace hgl::graph::mtl::SamplerName
{
    enum class SamplerSlot : uint8_t
    {
        BaseColor = 0,
        Normal,
        Tangent,
        Metallic,
        Roughness,
        Opacity,
        Text,
        Count
    };

    enum class TextureSampleVariant : uint8_t
    {
        Single2D = 0,
        Array2D,
        AtlasReserved
    };

    // Legacy names kept for compatibility with existing call sites.
    constexpr const char BaseColor[] = "TextureBaseColor";
    constexpr const char Normal[] = "TextureNormal";
    constexpr const char Tangent[] = "TextureTangent";
    constexpr const char Metallic[] = "TextureMetallic";
    constexpr const char Roughness[] = "TextureRoughness";
    constexpr const char Opacity[] = "TextureOpacity";
    constexpr const char Text[] = "TextureText";

    constexpr const char *ToDescriptorName(const SamplerSlot slot)
    {
        switch (slot)
        {
        case SamplerSlot::BaseColor:  return BaseColor;
        case SamplerSlot::Normal:     return Normal;
        case SamplerSlot::Tangent:    return Tangent;
        case SamplerSlot::Metallic:   return Metallic;
        case SamplerSlot::Roughness:  return Roughness;
        case SamplerSlot::Opacity:    return Opacity;
        case SamplerSlot::Text:       return Text;
        default:                      return "";
        }
    }

    // GLSL macro names for binding indirection.
    constexpr const char *ToBindingMacroName(const SamplerSlot slot)
    {
        switch (slot)
        {
        case SamplerSlot::BaseColor:  return "TEX_BASECOLOR_BINDING";
        case SamplerSlot::Normal:     return "TEX_NORMAL_BINDING";
        case SamplerSlot::Tangent:    return "TEX_TANGENT_BINDING";
        case SamplerSlot::Metallic:   return "TEX_METALLIC_BINDING";
        case SamplerSlot::Roughness:  return "TEX_ROUGHNESS_BINDING";
        case SamplerSlot::Opacity:    return "TEX_OPACITY_BINDING";
        case SamplerSlot::Text:       return "TEX_TEXT_BINDING";
        default:                      return "";
        }
    }

    // GLSL sampler symbol names.
    constexpr const char *ToGLSLSamplerSymbol(const SamplerSlot slot)
    {
        switch (slot)
        {
        case SamplerSlot::BaseColor:  return "Sampler_BaseColor";
        case SamplerSlot::Normal:     return "Sampler_Normal";
        case SamplerSlot::Tangent:    return "Sampler_Tangent";
        case SamplerSlot::Metallic:   return "Sampler_Metallic";
        case SamplerSlot::Roughness:  return "Sampler_Roughness";
        case SamplerSlot::Opacity:    return "Sampler_Opacity";
        case SamplerSlot::Text:       return "Sampler_Text";
        default:                      return "";
        }
    }

    // GLSL getter function names.
    constexpr const char *ToGLSLGetterName(const SamplerSlot slot)
    {
        switch (slot)
        {
        case SamplerSlot::BaseColor:  return "GetSamplerBaseColor";
        case SamplerSlot::Normal:     return "GetSamplerNormal";
        case SamplerSlot::Tangent:    return "GetSamplerTangent";
        case SamplerSlot::Metallic:   return "GetSamplerMetallic";
        case SamplerSlot::Roughness:  return "GetSamplerRoughness";
        case SamplerSlot::Opacity:    return "GetSamplerOpacity";
        case SamplerSlot::Text:       return "GetSamplerText";
        default:                      return "";
        }
    }

    constexpr const char *ToGLSLSamplerType(const TextureSampleVariant variant)
    {
        switch (variant)
        {
        case TextureSampleVariant::Single2D:      return "sampler2D";
        case TextureSampleVariant::Array2D:       return "sampler2DArray";
        case TextureSampleVariant::AtlasReserved: return "sampler2D"; // placeholder
        default:                                  return "sampler2D";
        }
    }

    inline bool TryGetSlotFromDescriptorName(const char *descriptor_name, SamplerSlot &slot)
    {
        if (!descriptor_name || !*descriptor_name)
            return false;

        for (uint8_t i = 0; i < static_cast<uint8_t>(SamplerSlot::Count); ++i)
        {
            const SamplerSlot current = static_cast<SamplerSlot>(i);
            const char *current_name = ToDescriptorName(current);
            if (current_name && std::strcmp(current_name, descriptor_name) == 0)
            {
                slot = current;
                return true;
            }
        }

        return false;
    }
}//namespace hgl::graph::mtl::SamplerName
