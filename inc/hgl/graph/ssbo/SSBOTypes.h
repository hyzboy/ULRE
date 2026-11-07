#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>

namespace hgl::graph
{
    /**
     * GlobalSSBOType - 全局单一数组池类型枚举
     * 统一管理由 GlobalSSBOBufferRegistry 托管的固定上限 Arena 行池
     * （MeshDrawParams + 材质表面字段；原 mtl::MaterialSSBOType 已并入本枚举）。
     * 每个类型在 GPU 侧拥有唯一持久 BDA，地址登记进全局地址表（GlobalAddresses SSBO，
     * 经 pc_root.addr_global_addresses 寻址——原 Set 0 的 GlobalAddressesInfo UBO 已退役）。
     *
     * `GeometryAABB` 是唯一**不自发行号**的类型：它与 `MeshDrawParams` **同 ID 索引**（一号双表），
     * 行号唯一来源 = MeshDrawParams 池，本池只把 [0, capacity) 静态置 Active（B3 定稿）。
     */
    enum class GlobalSSBOType : uint8_t
    {
        MeshDrawParams = 0,
        PBRSurface,
        EmissiveSurface,
        TransmissionSurface,
        PBRAdvancedSurface,     // Filament 高级材质扩展行（64 B）：前 32B 与 PBRSurfaceRow 逐字节同构
        GeometryAABB,           // 几何本地 AABB（32 B），与 MeshDrawParams 同 ID 索引的平行表
        MaterialData,           // 材质数据**组件行**（16 B）：组件在设备级的家（引用哪一行内容 + 授权标记）
        ModelTemplatePool,      // 模型模板行（128 B）：部件段索引 + LOD/阴影代理槽（GPU 常驻模板池，plan T1）
        ModelPartPool,          // 模型部件行（128 B）：模板内连续部件段（local_matrix + AABB + geometry/material id）

        ENUM_CLASS_RANGE(MeshDrawParams, ModelPartPool)
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
