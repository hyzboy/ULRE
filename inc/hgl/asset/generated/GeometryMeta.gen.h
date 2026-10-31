// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_NTBType(const hgl::graph::NTBType v)
    {
        switch (v)
        {
        case hgl::graph::NTBType::None: return "none";
        case hgl::graph::NTBType::Normal: return "normal";
        case hgl::graph::NTBType::NormalTangent: return "normal_tangent";
        case hgl::graph::NTBType::NormalTangentBitangent: return "normal_tangent_bitangent";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_NTBType(const char *s, hgl::graph::NTBType &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "none") == 0) { out = hgl::graph::NTBType::None; return true; }
        if (hgl::stricmp(s, "None") == 0) { out = hgl::graph::NTBType::None; return true; }
        if (hgl::stricmp(s, "normal") == 0) { out = hgl::graph::NTBType::Normal; return true; }
        if (hgl::stricmp(s, "Normal") == 0) { out = hgl::graph::NTBType::Normal; return true; }
        if (hgl::stricmp(s, "normal_tangent") == 0) { out = hgl::graph::NTBType::NormalTangent; return true; }
        if (hgl::stricmp(s, "NormalTangent") == 0) { out = hgl::graph::NTBType::NormalTangent; return true; }
        if (hgl::stricmp(s, "normal_tangent_bitangent") == 0) { out = hgl::graph::NTBType::NormalTangentBitangent; return true; }
        if (hgl::stricmp(s, "NormalTangentBitangent") == 0) { out = hgl::graph::NTBType::NormalTangentBitangent; return true; }
        return false;
    }

    inline const char *AssetEnumName64_NTBType(const int64_t v)
    { return AssetEnumName_NTBType(static_cast<hgl::graph::NTBType>(v)); }

    inline bool AssetEnumParse64_NTBType(const char *s, int64_t &out)
    {
        hgl::graph::NTBType tmp{};
        if (!AssetEnumParse_NTBType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_BuiltinSphereParams;
    extern const ClassMetaInfo Class_BuiltinDomeParams;
    extern const ClassMetaInfo Class_BuiltinCubeParams;
    extern const ClassMetaInfo Class_BuiltinPlaneGridParams;
    extern const ClassMetaInfo Class_BuiltinCylinderParams;
    extern const ClassMetaInfo Class_BuiltinHollowCylinderParams;
    extern const ClassMetaInfo Class_BuiltinTorusParams;
    extern const ClassMetaInfo Class_BuiltinConeParams;
    extern const ClassMetaInfo Class_BuiltinCapsuleParams;
    extern const ClassMetaInfo Class_BuiltinRectangleParams;
    extern const ClassMetaInfo Class_BuiltinBoundingBoxParams;
    extern const ClassMetaInfo Class_BuiltinHexSphereParams;
    extern const ClassMetaInfo Class_BuiltinTaperedCapsuleParams;
    extern const ClassMetaInfo Class_BuiltinFrustumParams;

    inline const PropertyMetaInfo Meta_BuiltinSphereParams[] =
    {
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinSphereParams =
    {
        "BuiltinSphereParams", "", "",
        Meta_BuiltinSphereParams, (uint32_t)(sizeof(Meta_BuiltinSphereParams) / sizeof(Meta_BuiltinSphereParams[0])),
        4u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinDomeParams[] =
    {
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinDomeParams =
    {
        "BuiltinDomeParams", "", "",
        Meta_BuiltinDomeParams, (uint32_t)(sizeof(Meta_BuiltinDomeParams) / sizeof(Meta_BuiltinDomeParams[0])),
        4u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinCubeParams[] =
    {
        { "segments_x", "segments_x", "segments_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_y", "segments_y", "segments_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_z", "segments_z", "segments_z", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
        { "tex_coord", "tex_coord", "tex_coord", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCubeParams =
    {
        "BuiltinCubeParams", "", "",
        Meta_BuiltinCubeParams, (uint32_t)(sizeof(Meta_BuiltinCubeParams) / sizeof(Meta_BuiltinCubeParams[0])),
        20u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinPlaneGridParams[] =
    {
        { "grid_x", "grid_x", "grid_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "grid_y", "grid_y", "grid_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_x", "sub_x", "sub_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_y", "sub_y", "sub_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "lum", "lum", "lum", "Default", "", "uint8_t", nullptr, nullptr, PropertyKind::UInt8, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 1u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_lum", "sub_lum", "sub_lum", "Default", "", "uint8_t", nullptr, nullptr, PropertyKind::UInt8, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 17u, 1u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "is_3d", "is_3d", "is_3d", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 18u, 2u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinPlaneGridParams =
    {
        "BuiltinPlaneGridParams", "", "",
        Meta_BuiltinPlaneGridParams, (uint32_t)(sizeof(Meta_BuiltinPlaneGridParams) / sizeof(Meta_BuiltinPlaneGridParams[0])),
        20u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinCylinderParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCylinderParams =
    {
        "BuiltinCylinderParams", "", "",
        Meta_BuiltinCylinderParams, (uint32_t)(sizeof(Meta_BuiltinCylinderParams) / sizeof(Meta_BuiltinCylinderParams[0])),
        12u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinHollowCylinderParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "inner_radius", "inner_radius", "inner_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "outer_radius", "outer_radius", "outer_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "cap_radial_tiles", "cap_radial_tiles", "cap_radial_tiles", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "cap_angular_tiles", "cap_angular_tiles", "cap_angular_tiles", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 20u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinHollowCylinderParams =
    {
        "BuiltinHollowCylinderParams", "", "",
        Meta_BuiltinHollowCylinderParams, (uint32_t)(sizeof(Meta_BuiltinHollowCylinderParams) / sizeof(Meta_BuiltinHollowCylinderParams[0])),
        24u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinTorusParams[] =
    {
        { "inner_radius", "inner_radius", "inner_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "outer_radius", "outer_radius", "outer_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_x", "uv_scale_x", "uv_scale_x", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_y", "uv_scale_y", "uv_scale_y", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 20u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinTorusParams =
    {
        "BuiltinTorusParams", "", "",
        Meta_BuiltinTorusParams, (uint32_t)(sizeof(Meta_BuiltinTorusParams) / sizeof(Meta_BuiltinTorusParams[0])),
        24u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinConeParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinConeParams =
    {
        "BuiltinConeParams", "", "",
        Meta_BuiltinConeParams, (uint32_t)(sizeof(Meta_BuiltinConeParams) / sizeof(Meta_BuiltinConeParams[0])),
        16u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinCapsuleParams[] =
    {
        { "half_height", "half_height", "half_height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCapsuleParams =
    {
        "BuiltinCapsuleParams", "", "",
        Meta_BuiltinCapsuleParams, (uint32_t)(sizeof(Meta_BuiltinCapsuleParams) / sizeof(Meta_BuiltinCapsuleParams[0])),
        16u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinRectangleParams[] =
    {
        { "left", "left", "left", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top", "top", "top", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "width", "width", "width", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "height", "height", "height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_x", "segments_x", "segments_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_y", "segments_y", "segments_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 20u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 24u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
        { "tex_coord", "tex_coord", "tex_coord", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinRectangleParams =
    {
        "BuiltinRectangleParams", "", "",
        Meta_BuiltinRectangleParams, (uint32_t)(sizeof(Meta_BuiltinRectangleParams) / sizeof(Meta_BuiltinRectangleParams[0])),
        32u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinBoundingBoxParams[] =
    {
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
    };

    inline const ClassMetaInfo Class_BuiltinBoundingBoxParams =
    {
        "BuiltinBoundingBoxParams", "", "",
        Meta_BuiltinBoundingBoxParams, (uint32_t)(sizeof(Meta_BuiltinBoundingBoxParams) / sizeof(Meta_BuiltinBoundingBoxParams[0])),
        4u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinHexSphereParams[] =
    {
        { "subdivisions", "subdivisions", "subdivisions", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_x", "uv_scale_x", "uv_scale_x", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_y", "uv_scale_y", "uv_scale_y", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinHexSphereParams =
    {
        "BuiltinHexSphereParams", "", "",
        Meta_BuiltinHexSphereParams, (uint32_t)(sizeof(Meta_BuiltinHexSphereParams) / sizeof(Meta_BuiltinHexSphereParams[0])),
        16u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinTaperedCapsuleParams[] =
    {
        { "half_height", "half_height", "half_height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "bottom_radius", "bottom_radius", "bottom_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top_radius", "top_radius", "top_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinTaperedCapsuleParams =
    {
        "BuiltinTaperedCapsuleParams", "", "",
        Meta_BuiltinTaperedCapsuleParams, (uint32_t)(sizeof(Meta_BuiltinTaperedCapsuleParams) / sizeof(Meta_BuiltinTaperedCapsuleParams[0])),
        20u, 4u
    };

    inline const PropertyMetaInfo Meta_BuiltinFrustumParams[] =
    {
        { "bottom_radius", "bottom_radius", "bottom_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top_radius", "top_radius", "top_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "height", "height", "height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 12u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "generate_caps", "generate_caps", "generate_caps", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 16u, 4u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinFrustumParams =
    {
        "BuiltinFrustumParams", "", "",
        Meta_BuiltinFrustumParams, (uint32_t)(sizeof(Meta_BuiltinFrustumParams) / sizeof(Meta_BuiltinFrustumParams[0])),
        20u, 4u
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinSphereParams *) { return &Class_BuiltinSphereParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinDomeParams *) { return &Class_BuiltinDomeParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinCubeParams *) { return &Class_BuiltinCubeParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinPlaneGridParams *) { return &Class_BuiltinPlaneGridParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinCylinderParams *) { return &Class_BuiltinCylinderParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinHollowCylinderParams *) { return &Class_BuiltinHollowCylinderParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinTorusParams *) { return &Class_BuiltinTorusParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinConeParams *) { return &Class_BuiltinConeParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinCapsuleParams *) { return &Class_BuiltinCapsuleParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinRectangleParams *) { return &Class_BuiltinRectangleParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinBoundingBoxParams *) { return &Class_BuiltinBoundingBoxParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinHexSphereParams *) { return &Class_BuiltinHexSphereParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinTaperedCapsuleParams *) { return &Class_BuiltinTaperedCapsuleParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinFrustumParams *) { return &Class_BuiltinFrustumParams; }
} // namespace hgl::asset::generated
