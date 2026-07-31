#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/TextureSlot.h>

namespace hgl::graph::mtl
{
    // SSBO 类型枚举：用于在 Recipe/Spec 中以稳定整数传递“结构体数据落在哪类缓冲”。
    enum class SSBOType : uint16_t
    {
        TextureLayer = 0,
        MaterialSSBOIndexTable,
        PBRSurface,
        EmissiveSurface,
        ClearCoatSurface,
        TransmissionSurface,
        TransformIndexRows,
        LocalToWorld,
        UserDefined,

        ENUM_CLASS_RANGE(TextureLayer, UserDefined)
    };

    using SSBOCategory = SSBOType;
    constexpr uint32_t MaterialSSBOSlotCount = 6;

    constexpr bool IsMaterialSSBOType(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::PBRSurface:
        case SSBOType::EmissiveSurface:
        case SSBOType::ClearCoatSurface:
        case SSBOType::TransmissionSurface:
        case SSBOType::UserDefined:
            return true;
        default:
            return false;
        }
    }

    constexpr uint32_t GetSSBOSlotByType(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::PBRSurface: return 0;
        case SSBOType::EmissiveSurface: return 1;
        case SSBOType::ClearCoatSurface: return 2;
        case SSBOType::TransmissionSurface: return 3;
        default: return 4;
        }
    }

    constexpr SSBOType GetSSBOTypeBySlot(const uint32_t ssbo_slot) noexcept
    {
        switch (ssbo_slot)
        {
        case 0: return SSBOType::PBRSurface;
        case 1: return SSBOType::EmissiveSurface;
        case 2: return SSBOType::ClearCoatSurface;
        case 3: return SSBOType::TransmissionSurface;
        default: return SSBOType::UserDefined;
        }
    }

    inline const char *GetSSBOTypeName(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer: return "TextureLayer";
        case SSBOType::MaterialSSBOIndexTable: return "MaterialSSBOIndexTable";
        case SSBOType::PBRSurface: return "PBRSurface";
        case SSBOType::EmissiveSurface: return "EmissiveSurface";
        case SSBOType::ClearCoatSurface: return "ClearCoatSurface";
        case SSBOType::TransmissionSurface: return "TransmissionSurface";
        case SSBOType::TransformIndexRows: return "TransformIndexRows";
        case SSBOType::LocalToWorld: return "LocalToWorld";
        case SSBOType::UserDefined: return "UserDefined";
        default: return "Unknown";
        }
    }

    inline uint32_t GetSSBOTypeStructVersion(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer:
        case SSBOType::MaterialSSBOIndexTable:
        case SSBOType::TransformIndexRows:
        case SSBOType::LocalToWorld:
            return 1;
        default:
            break;
        }

        return 0;
    }

    inline uint32_t GetSSBOTypeStructStride(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer:
            return sizeof(uint32_t) * static_cast<uint32_t>(TextureSlot::RANGE_SIZE);
        case SSBOType::MaterialSSBOIndexTable:
            return sizeof(uint32_t) * MaterialSSBOSlotCount;
        case SSBOType::TransformIndexRows:
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
        constexpr uint32_t TransformIndexRows   = MakeECSSSBOId(1);
        constexpr uint32_t LocalToWorldData     = MakeECSSSBOId(2);

        constexpr uint32_t MaterialInstanceRows = MakeECSSSBOId(3);
        constexpr uint32_t MaterialInstanceData = MakeECSSSBOId(4);
        constexpr uint32_t TextureLayerRows     = MakeECSSSBOId(5);
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

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t ssbo_slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, ssbo_slot};
    }

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const TextureSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }
}
