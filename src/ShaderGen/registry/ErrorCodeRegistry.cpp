#include <hgl/shadergen/registry/ErrorCodeRegistry.h>
#include <cstdio>

namespace hgl::graph::mtl
{
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

} // namespace hgl::graph::mtl
