#include <hgl/shadergen/registry/ErrorCodeRegistry.h>
#include <cstdio>
#include <array>
#include <cctype>

namespace hgl::graph::mtl
{
    namespace
    {
        static constexpr std::array<std::string_view, 5> kSFMAllowedKeys = {
            "surface_type",
            "supports_phase",
            "require",
            "optional",
            "derive",
        };

        static std::string NormalizeSFMKey(std::string_view key)
        {
            // Accept both "@sfm:require" and "require".
            if (key.size() >= 5)
            {
                const std::string_view prefix = key.substr(0, 5);
                if (prefix == "@sfm:")
                    key.remove_prefix(5);
            }

            std::string out;
            out.reserve(key.size());
            for (char c : key)
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return out;
        }
    }

    const char *GetFSErrorReasonName(FSErrorReason r) noexcept
    {
        switch (r)
        {
            case FSErrorReason::Unknown:             return "Unknown";
            case FSErrorReason::NoVariantRegistered: return "NoVariantRegistered";
            case FSErrorReason::NoSurfaceVariant:    return "NoSurfaceVariant";
            case FSErrorReason::NoVSTemplate:        return "NoVSTemplate";
            case FSErrorReason::NoFSTemplate:        return "NoFSTemplate";
            case FSErrorReason::AssemblyFailed:      return "AssemblyFailed";
            case FSErrorReason::FactoryTypeMissing:  return "FactoryTypeMissing";
            case FSErrorReason::FactoryDispatchFail: return "FactoryDispatchFail";
            default:                                 return "<invalid>";
        }
    }

    std::string FormatFSError(uint32_t error_code)
    {
        const DecodedFSError d = DecodeFSError(error_code);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "reason=%s surface_model=%u tex_bits_lo=0x%02X sampler_bits_lo=0x%02X (raw=0x%08X)",
            GetFSErrorReasonName(d.reason),
            static_cast<unsigned>(d.surface_model_id),
            static_cast<unsigned>(d.tex_bits_lo),
            static_cast<unsigned>(d.sampler_bits_lo),
            error_code);
        return buf;
    }

    const char *GetSFMAnnotationErrorName(SFMAnnotationError e) noexcept
    {
        switch (e)
        {
            case SFMAnnotationError::None:             return "None";
            case SFMAnnotationError::UnknownKey:       return "UnknownKey";
            case SFMAnnotationError::DuplicateKey:     return "DuplicateKey";
            case SFMAnnotationError::ConflictingKey:   return "ConflictingKey";
            case SFMAnnotationError::DeriveOutOfRange: return "DeriveOutOfRange";
            case SFMAnnotationError::InvalidDirective: return "InvalidDirective";
            default:                                   return "<invalid>";
        }
    }

    uint8_t GetSFMAnnotationKeyIndex(std::string_view key) noexcept
    {
        const std::string normalized = NormalizeSFMKey(key);

        for (size_t i = 0; i < kSFMAllowedKeys.size(); ++i)
        {
            if (normalized == kSFMAllowedKeys[i])
                return static_cast<uint8_t>(i);
        }

        return 0xFFu;
    }

    bool IsKnownSFMAnnotationKey(std::string_view key) noexcept
    {
        return GetSFMAnnotationKeyIndex(key) != 0xFFu;
    }

    std::string_view GetSFMAnnotationKeyName(uint8_t key_index) noexcept
    {
        if (key_index >= kSFMAllowedKeys.size())
            return {};
        return kSFMAllowedKeys[key_index];
    }

    std::string FormatSFMAnnotationError(uint32_t error_code)
    {
        const DecodedSFMAnnotationError d = DecodeSFMAnnotationError(error_code);
        const std::string_view key_name = GetSFMAnnotationKeyName(d.key_index);

        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "error=%s key_index=%u key=%.*s line_mod_256=%u (raw=0x%08X)",
            GetSFMAnnotationErrorName(d.error),
            static_cast<unsigned>(d.key_index),
            static_cast<int>(key_name.size()),
            key_name.data() ? key_name.data() : "",
            static_cast<unsigned>(d.line_mod_256),
            error_code);
        return buf;
    }

} // namespace hgl::graph::mtl
