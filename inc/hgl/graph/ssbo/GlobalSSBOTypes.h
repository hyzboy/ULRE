#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph
{
    // GlobalSSBOType 枚举定义于 SSBOTypes.h（供仅含 SSBOTypes.h 的 mtl 头直接使用）。

    constexpr bool IsGlobalSSBOType(const GlobalSSBOType type) noexcept
    {
        return type >= GlobalSSBOType::BEGIN_RANGE && type <= GlobalSSBOType::END_RANGE;
    }

    inline const char *GetGlobalSSBOTypeName(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::MeshDrawParams:        return "MeshDrawParams";
        case GlobalSSBOType::PBRSurface:           return "PBRSurface";
        case GlobalSSBOType::EmissiveSurface:      return "EmissiveSurface";
        case GlobalSSBOType::TransmissionSurface:  return "TransmissionSurface";
        default:                                   return "UnknownGlobalSSBO";
        }
    }

    inline uint32_t GetGlobalSSBOTypeStructVersion(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::PBRSurface:
        case GlobalSSBOType::EmissiveSurface:
        case GlobalSSBOType::TransmissionSurface:
            return 1;
        default:
            break;
        }

        return 0;
    }

    inline uint32_t GetGlobalSSBOTypeStructStride(const GlobalSSBOType type) noexcept
    {
        switch (type)
        {
        case GlobalSSBOType::PBRSurface:            return sizeof(float) * 8;
        case GlobalSSBOType::EmissiveSurface:       return sizeof(float) * 4;
        case GlobalSSBOType::TransmissionSurface:   return sizeof(uint32_t) * 4;
        default:
            break;
        }

        return 0;
    }

    /**
     * Identifies one live global-data row in a shared global SSBO.
     * The type selects the physical backing buffer; data_index selects its row.
     */
    struct GlobalSSBOBinding
    {
        GlobalSSBOType ssbo_type = GlobalSSBOType::PBRSurface;
        uint32_t ssbo_id = 0;
        uint32_t data_index = uint32_t(-1);

        constexpr bool IsValid() const noexcept
        {
            return IsGlobalSSBOType(ssbo_type)
                && ssbo_id != 0
                && data_index != uint32_t(-1);
        }
    };

    struct GlobalSSBOConfig
    {
        GlobalSSBOType type;
        const char *name;
        uint32_t row_bytes;
        uint32_t default_capacity;
        uint32_t reserve_rows;
    };
}
