#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/TextureSlot.h>

namespace hgl::graph::mtl
{
    // 逻辑结构体数据槽位（用于声明“实例参数属于哪一类数据语义”）。
    enum class DataSlot : uint8_t
    {
        PBRSurface = 0,
        EmissiveSurface,
        ClearCoatSurface,
        TransmissionSurface,
        User0,
        User1,

        ENUM_CLASS_RANGE(PBRSurface, User1)
    };

    // SSBO 类型枚举：用于在 Recipe/Spec 中以稳定整数传递“结构体数据落在哪类缓冲”。
    enum class SSBOType : uint16_t
    {
        TextureLayer = 0,
        DataIndex,
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

    inline const char *GetSSBOTypeName(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer: return "TextureLayer";
        case SSBOType::DataIndex: return "DataIndex";
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
        case SSBOType::DataIndex:
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
        case SSBOType::DataIndex:
            return sizeof(uint32_t) * static_cast<uint32_t>(DataSlot::RANGE_SIZE);
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

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const DataSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const TextureSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }
}
