#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/CoreType.h>
#include <hgl/graph/ssbo/TextureSlot.h>

namespace hgl::graph::mtl
{
    // SSBO 类型枚举：用于在 Recipe/Spec 中以稳定整数传递“结构体数据落在哪类缓冲”。
    enum class SSBOType : uint16_t
    {
        MeshDrawParams=0,
        // 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）
        VertexPosition,
        VertexUV,
        VertexNTB,
        VertexColor,
        VertexLuminance,
        VertexTransformID,
        VertexSize,
        VertexIndex,

        TransformIndexRows,
        LocalToWorld,

        UserDefined,
        MaterialPrivateDataIndexTable,

        TextureLayer,

        PBRSurface,
        EmissiveSurface,
        TextureRectArraySurface,
        TransmissionSurface,

        ENUM_CLASS_RANGE(MeshDrawParams,TransmissionSurface)
    };

    using SSBOCategory = SSBOType;
    constexpr uint32_t DefaultMaterialPrivateDataSlot = 0;
    // 单槽化：一个材质只有一个私有数据 SSBO（MaterialPrivateData）。
    // 槽位数恒为 1，行表写单列。
    constexpr uint32_t MaxMaterialPrivateDataSlotsPerMaterial = 1u;
    constexpr uint32_t MaterialPrivateDataIndexRowStride = MaxMaterialPrivateDataSlotsPerMaterial;

    constexpr bool IsMaterialSSBOType(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::PBRSurface:
        case SSBOType::EmissiveSurface:
        case SSBOType::TextureRectArraySurface:
        case SSBOType::TransmissionSurface:
        case SSBOType::UserDefined:
            return true;
        default:
            return false;
        }
    }

    inline const char *GetSSBOTypeName(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::MeshDrawParams: return "MeshDrawParams";
        case SSBOType::TextureLayer: return "TextureLayer";
        case SSBOType::MaterialPrivateDataIndexTable: return "MaterialPrivateDataIndexTable";
        case SSBOType::PBRSurface: return "PBRSurface";
        case SSBOType::EmissiveSurface: return "EmissiveSurface";
        case SSBOType::TextureRectArraySurface: return "TextureRectArraySurface";
        case SSBOType::TransmissionSurface: return "TransmissionSurface";
        case SSBOType::TransformIndexRows: return "TransformIndexRows";
        case SSBOType::LocalToWorld: return "LocalToWorld";
        case SSBOType::UserDefined: return "UserDefined";
        case SSBOType::VertexPosition: return "VertexPosition";
        case SSBOType::VertexUV: return "VertexUV";
        case SSBOType::VertexNTB: return "VertexNTB";
        case SSBOType::VertexColor: return "VertexColor";
        case SSBOType::VertexLuminance: return "VertexLuminance";
        case SSBOType::VertexTransformID: return "VertexTransformID";
        case SSBOType::VertexSize: return "VertexSize";
        case SSBOType::VertexIndex: return "VertexIndex";
        default: return "Unknown";
        }
    }

    inline uint32_t GetSSBOTypeStructVersion(const SSBOType type) noexcept
    {
        switch (type)
        {
        case SSBOType::TextureLayer:
        case SSBOType::MaterialPrivateDataIndexTable:
        case SSBOType::TransformIndexRows:
        case SSBOType::LocalToWorld:
        case SSBOType::PBRSurface:
        case SSBOType::EmissiveSurface:
        case SSBOType::TextureRectArraySurface:
        case SSBOType::TransmissionSurface:
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
        case SSBOType::MaterialPrivateDataIndexTable:
            return 0;  // dynamic: stride = material_private_data_slot_decls.size() * sizeof(uint32_t) per material
        case SSBOType::PBRSurface:
            return sizeof(float) * 8; // vec4 base_color + metallic + roughness + normal_scale + fresnel
        case SSBOType::EmissiveSurface:
            return sizeof(float) * 4;                    // vec4/uvec4 style payload
        case SSBOType::TextureRectArraySurface:
            return sizeof(uint32_t) * 4;                // uvec4 id
        case SSBOType::TransmissionSurface:
            return sizeof(uint32_t);                     // packed uint payload
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

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const uint32_t material_private_data_slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, material_private_data_slot};
    }

    inline SSBOAddress MakeSSBOAddress(const SSBOType ssbo_type, const uint32_t ssbo_id, const TextureSlot slot) noexcept
    {
        return SSBOAddress{ssbo_type, ssbo_id, static_cast<uint32_t>(slot)};
    }
}
