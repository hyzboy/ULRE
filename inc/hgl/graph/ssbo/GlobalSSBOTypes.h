#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/SSBOTypes.h>

namespace hgl::graph
{
    /**
     * GlobalSSBOType - 全局单一数组池类型枚举
     * 用于统一管理由 GlobalSSBOBufferRegistry 托管的固定上限 Arena 行池。
     * 每个类型在 GPU 侧拥有唯一持久 BDA，并映射至 Set 0 Binding 4 GlobalAddressesInfo。
     */
    enum class GlobalSSBOType : uint8_t
    {
        MeshDrawParams = 0,
        PBRSurface,
        EmissiveSurface,
        TransmissionSurface,

        ENUM_CLASS_RANGE(MeshDrawParams, TransmissionSurface)
    };

    constexpr uint32_t GlobalSSBOTypeCount = static_cast<uint32_t>(GlobalSSBOType::RANGE_SIZE);

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

    inline GlobalSSBOType ToGlobalSSBOType(const mtl::MaterialSSBOType mat_type) noexcept
    {
        switch (mat_type)
        {
        case mtl::MaterialSSBOType::PBRSurface:         return GlobalSSBOType::PBRSurface;
        case mtl::MaterialSSBOType::EmissiveSurface:    return GlobalSSBOType::EmissiveSurface;
        case mtl::MaterialSSBOType::TransmissionSurface:return GlobalSSBOType::TransmissionSurface;
        default:                                        return GlobalSSBOType::PBRSurface;
        }
    }

    inline mtl::MaterialSSBOType ToMaterialSSBOType(const GlobalSSBOType global_type) noexcept
    {
        switch (global_type)
        {
        case GlobalSSBOType::PBRSurface:         return mtl::MaterialSSBOType::PBRSurface;
        case GlobalSSBOType::EmissiveSurface:    return mtl::MaterialSSBOType::EmissiveSurface;
        case GlobalSSBOType::TransmissionSurface:return mtl::MaterialSSBOType::TransmissionSurface;
        default:                                 return mtl::MaterialSSBOType::PBRSurface;
        }
    }

    struct GlobalSSBOConfig
    {
        GlobalSSBOType type;
        const char *name;
        uint32_t row_bytes;
        uint32_t default_capacity;
        uint32_t reserve_rows;
    };
}
