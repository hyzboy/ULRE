#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <string>

namespace hgl::graph::mtl
{
    enum class SamplerSlot : uint8_t
    {
        BaseColor = 0,
        Normal,
        Tangent,
        Metallic,
        Roughness,
        Height,
        Opacity,
        Text,

        ENUM_CLASS_RANGE(BaseColor, Text)
    };

    enum class TextureSourceMode : uint8
    {
        None = 0,
        Simple,
        Array,
        Atlas,

        ENUM_CLASS_RANGE(None, Atlas)
    };

    constexpr size_t SamplerSlotCount = size_t(SamplerSlot::RANGE_SIZE);

    // The single authoritative slot-name list (must match SamplerSlot order).
    constexpr const char *SamplerSlotNameList[] =
    {
        "BaseColor",
        "Normal",
        "Tangent",
        "Metallic",
        "Roughness",
        "Height",
        "Opacity",
        "Text",
    };

    static_assert(sizeof(SamplerSlotNameList) / sizeof(SamplerSlotNameList[0]) == SamplerSlotCount,
                  "SamplerSlotNameList must match SamplerSlot enum order");

    inline std::string ToUpperASCII(const char *text)
    {
        std::string result;
        if (!text || !*text)
            return result;

        while (*text)
        {
            const unsigned char c = static_cast<unsigned char>(*text);
            result.push_back((c >= 'a' && c <= 'z') ? char(c - ('a' - 'A')) : char(c));
            ++text;
        }

        return result;
    }

    inline const std::array<std::string, SamplerSlotCount> &GetSamplerDescriptorNameCache()
    {
        static const std::array<std::string, SamplerSlotCount> cache = []
        {
            std::array<std::string, SamplerSlotCount> names{};

            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                names[i] = "Texture";
                names[i] += SamplerSlotNameList[i];
            }

            return names;
        }();

        return cache;
    }

    inline const std::array<std::string, SamplerSlotCount> &GetSamplerBindingMacroNameCache()
    {
        static const std::array<std::string, SamplerSlotCount> cache = []
        {
            std::array<std::string, SamplerSlotCount> names{};

            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                const char *base = SamplerSlotNameList[i];

                names[i] = "TEX_";
                names[i] += ToUpperASCII(base);
                names[i] += "_BINDING";
            }

            return names;
        }();

        return cache;
    }

    inline const std::array<std::string, SamplerSlotCount> &GetGLSLSamplerSymbolCache()
    {
        static const std::array<std::string, SamplerSlotCount> cache = []
        {
            std::array<std::string, SamplerSlotCount> names{};

            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                names[i] = "Sampler_";
                names[i] += SamplerSlotNameList[i];
            }

            return names;
        }();

        return cache;
    }

    inline const std::array<std::string, SamplerSlotCount> &GetGLSLGetSamplerCache()
    {
        static const std::array<std::string, SamplerSlotCount> cache = []
        {
            std::array<std::string, SamplerSlotCount> names{};

            for (size_t i = 0; i < SamplerSlotCount; ++i)
            {
                names[i] = "GetSampler";
                names[i] += SamplerSlotNameList[i];
            }

            return names;
        }();

        return cache;
    }

    inline const char *ToDescriptorName(const SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        if (index >= SamplerSlotCount)
            return "";

        return GetSamplerDescriptorNameCache()[index].c_str();
    }

    inline const char *ToBindingMacroName(const SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        if (index >= SamplerSlotCount)
            return "";

        return GetSamplerBindingMacroNameCache()[index].c_str();
    }

    inline const char *ToGLSLSamplerSymbol(const SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        if (index >= SamplerSlotCount)
            return "";

        return GetGLSLSamplerSymbolCache()[index].c_str();
    }

    inline const char *ToGLSLGetterName(const SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        if (index >= SamplerSlotCount)
            return "";

        return GetGLSLGetSamplerCache()[index].c_str();
    }

    constexpr const char *ToGLSLSamplerType(const TextureSourceMode mode)
    {
        switch (mode)
        {
        case TextureSourceMode::Array:   return "sampler2DArray";
        case TextureSourceMode::Simple:  return "sampler2D";
        case TextureSourceMode::Atlas:   return "sampler2D"; // placeholder
        case TextureSourceMode::None:    return "sampler2D";
        default:                         return "sampler2D";
        }
    }

    inline bool TryGetSlotFromDescriptorName(const char *descriptor_name, SamplerSlot &slot)
    {
        if (!descriptor_name || !*descriptor_name)
            return false;

        for (uint8_t i = 0; i < static_cast<uint8_t>(SamplerSlot::RANGE_SIZE); ++i)
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
}//namespace hgl::graph::mtl
