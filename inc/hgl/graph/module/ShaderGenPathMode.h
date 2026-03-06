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
            return ShaderGenPathMode::LegacyOnly;

        if (std::strcmp(mode, "legacy-only") == 0)
            return ShaderGenPathMode::LegacyOnly;

        if (std::strcmp(mode, "mirror-preferred") == 0)
            return ShaderGenPathMode::MirrorPreferred;

        return ShaderGenPathMode::LegacyOnly;
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

    inline bool IsShaderGenFullDiffLogEnabled(const ShaderGenPathMode mode)
    {
        return mode == ShaderGenPathMode::MirrorPreferred;
    }

    struct ShaderGenPathPolicy
    {
        bool enable_mirror_validation = true;
        bool require_mirror_valid = false;
        bool full_diff_log = false;
    };

    inline ShaderGenPathPolicy MakeShaderGenPathPolicy(const ShaderGenPathMode mode)
    {
        ShaderGenPathPolicy policy;
        policy.enable_mirror_validation = (mode != ShaderGenPathMode::LegacyOnly);
        policy.require_mirror_valid = (mode == ShaderGenPathMode::MirrorPreferred);
        policy.full_diff_log = IsShaderGenFullDiffLogEnabled(mode);
        return policy;
    }
}//namespace hgl::graph
