#pragma once

#include <cstring>

namespace hgl::graph
{
    enum class ShaderGenPathMode
    {
        LegacyOnly,
        MirrorValidate,
        MirrorPreferred,
    };

    inline ShaderGenPathMode ParseShaderGenPathMode(const char *mode)
    {
        if (!mode || !mode[0])
            return ShaderGenPathMode::MirrorValidate;

        if (std::strcmp(mode, "legacy-only") == 0)
            return ShaderGenPathMode::LegacyOnly;

        if (std::strcmp(mode, "mirror-preferred") == 0)
            return ShaderGenPathMode::MirrorPreferred;

        return ShaderGenPathMode::MirrorValidate;
    }

    inline const char *GetShaderGenPathModeName(const ShaderGenPathMode mode)
    {
        switch(mode)
        {
            case ShaderGenPathMode::LegacyOnly: return "legacy-only";
            case ShaderGenPathMode::MirrorPreferred: return "mirror-preferred";
            case ShaderGenPathMode::MirrorValidate:
            default: return "mirror-validate";
        }
    }
}//namespace hgl::graph
