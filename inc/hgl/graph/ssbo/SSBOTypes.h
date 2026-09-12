#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>

namespace hgl::graph::mtl
{
    enum class MaterialSSBOType : uint16_t
    {
        PBRSurface = 0,
        EmissiveSurface,
        TransmissionSurface,

        ENUM_CLASS_RANGE(PBRSurface, TransmissionSurface)
    };

    // SSBO 类型枚举：用于在 Recipe/Spec 中以稳定整数传递“结构体数据落在哪类缓冲”。
    enum class SSBOType : uint16_t
    {
        MeshDrawParams=0,

        LocalToWorld,
        LocalToWorldIndex,

        UserDefined,

        ENUM_CLASS_RANGE(MeshDrawParams,UserDefined)
    };

    using SSBOCategory = SSBOType;
    constexpr bool IsMaterialSSBOType(const MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case MaterialSSBOType::PBRSurface:
        case MaterialSSBOType::EmissiveSurface:
        case MaterialSSBOType::TransmissionSurface:
            return true;
        default:
            return false;
        }
    }

    /**
     * Identifies one live material-data row in a shared material SSBO.
     * The type selects the physical backing buffer; data_index selects its row.
     */
    struct MaterialSSBOBinding
    {
        MaterialSSBOType ssbo_type = MaterialSSBOType::PBRSurface;
        uint32_t ssbo_id = 0;
        uint32_t data_index = uint32_t(-1);

        constexpr bool IsValid() const noexcept
        {
            return IsMaterialSSBOType(ssbo_type)
                && ssbo_id != 0
                && data_index != uint32_t(-1);
        }
    };

    inline const char *GetMaterialSSBOTypeName(const MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case MaterialSSBOType::PBRSurface: return "PBRSurface";
        case MaterialSSBOType::EmissiveSurface: return "EmissiveSurface";
        case MaterialSSBOType::TransmissionSurface: return "TransmissionSurface";
        default: return "UnknownMaterialSSBO";
        }
    }

    inline const char *GetSSBOTypeName(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::MeshDrawParams: return "MeshDrawParams";
        case SSBOType::LocalToWorldIndex: return "LocalToWorldIndex";
        case SSBOType::LocalToWorld: return "LocalToWorld";
        case SSBOType::UserDefined: return "UserDefined";
        default: return "Unknown";
        }
    }

    inline uint32_t GetMaterialSSBOTypeStructVersion(const MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case MaterialSSBOType::PBRSurface:
        case MaterialSSBOType::EmissiveSurface:
        case MaterialSSBOType::TransmissionSurface:
            return 1;
        default:
            break;
        }

        return 0;
    }

    inline uint32_t GetSSBOTypeStructVersion(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::LocalToWorldIndex:
        case SSBOType::LocalToWorld:
            return 1;
        default:
            break;
        }

        return 0;
    }

    inline uint32_t GetMaterialSSBOTypeStructStride(const MaterialSSBOType type) noexcept
    {
        switch (type)
        {
        case MaterialSSBOType::PBRSurface:
            return sizeof(float) * 8;
        case MaterialSSBOType::EmissiveSurface:
            return sizeof(float) * 4;
        case MaterialSSBOType::TransmissionSurface:
            return sizeof(uint32_t) * 4;
        default:
            break;
        }

        return 0;
    }

    inline uint32_t GetSSBOTypeStructStride(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::LocalToWorldIndex:
            return sizeof(uint32_t);
        case SSBOType::LocalToWorld:
            return sizeof(float) * 16;
        default:
            break;
        }

        return 0;
    }

    constexpr uint32_t SSBOIdNamespaceBit = 0x80000000u;
    constexpr uint32_t SSBOIdLocalMask = 0x7fffffffu;

    constexpr uint32_t MakeRecipeSSBOId(const uint32_t local_id) noexcept
    {
        return local_id & SSBOIdLocalMask;
    }

    constexpr uint32_t MakeECSSSBOId(const uint32_t local_id) noexcept
    {
        return (local_id & SSBOIdLocalMask) | SSBOIdNamespaceBit;
    }

    constexpr bool IsECSSSBOId(const uint32_t ssbo_id) noexcept
    {
        return (ssbo_id & SSBOIdNamespaceBit) != 0;
    }

    constexpr uint32_t GetSSBOIdLocalPart(const uint32_t ssbo_id) noexcept
    {
        return ssbo_id & SSBOIdLocalMask;
    }

    namespace ECSReservedSSBOId
    {
        constexpr uint32_t LocalToWorldIndex   = MakeECSSSBOId(1);
        constexpr uint32_t LocalToWorldData     = MakeECSSSBOId(2);
    }

    struct SSBOAddress
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id = 0;
        uint32_t slot = 0;
    };

    struct SSBOBinding
    {
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32_t ssbo_id   = 0;

        bool IsValid() const
        {
            return ssbo_id != 0 || ssbo_type != SSBOType::UserDefined;
        }
    };

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, slot};
    }

}
