#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/type/StrChar.h>

namespace hgl::graph
{
    enum class PassType : uint8
    {
        ForwardOpaque = 0,
        ForwardMasked,
        ForwardTransparent,
        ForwardDither,
        ForwardA2C,
        ShadowOpaque,
        ShadowMasked,
        EarlyZSolid,
        EarlyZMasked,

        ENUM_CLASS_RANGE(ForwardOpaque, EarlyZMasked)
    };

    // ── PassType 名字注册表（单一真源，T1）──────────────────────────
    // 旧 MaterialDefinitionFile.cpp 的 ParsePass 平行表已删除。
    inline const char *GetPassTypeName(const PassType pass) noexcept
    {
        switch (pass)
        {
        case PassType::ForwardOpaque:      return "ForwardOpaque";
        case PassType::ForwardMasked:      return "ForwardMasked";
        case PassType::ForwardTransparent: return "ForwardTransparent";
        case PassType::ForwardDither:      return "ForwardDither";
        case PassType::ForwardA2C:         return "ForwardA2C";
        case PassType::ShadowOpaque:       return "ShadowOpaque";
        case PassType::ShadowMasked:       return "ShadowMasked";
        case PassType::EarlyZSolid:        return "EarlyZSolid";
        case PassType::EarlyZMasked:       return "EarlyZMasked";
        default:                           return "Unknown";
        }
    }

    inline bool ParsePassType(const char *name, PassType &out_pass) noexcept
    {
        if (!name || !name[0])
        {
            out_pass = PassType::ForwardOpaque;
            return false;
        }

        ENUM_CLASS_FOR(PassType, int, i)
        {
            const PassType pass = static_cast<PassType>(i);
            if (hgl::strcmp(name, GetPassTypeName(pass)) == 0)
            {
                out_pass = pass;
                return true;
            }
        }

        out_pass = PassType::ForwardOpaque;
        return false;
    }
}
