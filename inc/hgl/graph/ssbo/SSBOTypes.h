#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>

namespace hgl::graph
{
    /**
     * GlobalSSBOType - 全局单一数组池类型枚举
     * 统一管理由 GlobalSSBOBufferRegistry 托管的固定上限 Arena 行池
     * （MeshDrawParams + 材质表面字段；原 mtl::MaterialSSBOType 已并入本枚举）。
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
}

namespace hgl::graph::mtl
{
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

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, slot};
    }

}
