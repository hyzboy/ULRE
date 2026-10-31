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

    inline const char *AssetEnumName_VertexNormalFormat(const hgl::graph::VertexNormalFormat v)
    { return hgl::graph::GetVertexNormalFormatName(v); }

    inline bool AssetEnumParse_VertexNormalFormat(const char *s, hgl::graph::VertexNormalFormat &out)
    { return hgl::graph::ParseVertexNormalFormat(s, out); }

    inline const char *AssetEnumName_GeometryAssetKind(const hgl::graph::GeometryAssetKind v)
    { return hgl::graph::GetGeometryAssetKindName(v); }

    inline bool AssetEnumParse_GeometryAssetKind(const char *s, hgl::graph::GeometryAssetKind &out)
    { return hgl::graph::ParseGeometryAssetKind(s, out); }

    inline const char *AssetEnumName_BuiltinShapeKind(const hgl::graph::BuiltinShapeKind v)
    { return hgl::graph::GetBuiltinShapeKindName(v); }

    inline bool AssetEnumParse_BuiltinShapeKind(const char *s, hgl::graph::BuiltinShapeKind &out)
    { return hgl::graph::ParseBuiltinShapeKind(s, out); }

    inline const char *AssetEnumName64_NTBType(const int64_t v)
    { return AssetEnumName_NTBType(static_cast<hgl::graph::NTBType>(v)); }

    inline bool AssetEnumParse64_NTBType(const char *s, int64_t &out)
    {
        hgl::graph::NTBType tmp{};
        if (!AssetEnumParse_NTBType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_VertexNormalFormat(const int64_t v)
    { return AssetEnumName_VertexNormalFormat(static_cast<hgl::graph::VertexNormalFormat>(v)); }

    inline bool AssetEnumParse64_VertexNormalFormat(const char *s, int64_t &out)
    {
        hgl::graph::VertexNormalFormat tmp{};
        if (!AssetEnumParse_VertexNormalFormat(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_GeometryAssetKind(const int64_t v)
    { return AssetEnumName_GeometryAssetKind(static_cast<hgl::graph::GeometryAssetKind>(v)); }

    inline bool AssetEnumParse64_GeometryAssetKind(const char *s, int64_t &out)
    {
        hgl::graph::GeometryAssetKind tmp{};
        if (!AssetEnumParse_GeometryAssetKind(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_BuiltinShapeKind(const int64_t v)
    { return AssetEnumName_BuiltinShapeKind(static_cast<hgl::graph::BuiltinShapeKind>(v)); }

    inline bool AssetEnumParse64_BuiltinShapeKind(const char *s, int64_t &out)
    {
        hgl::graph::BuiltinShapeKind tmp{};
        if (!AssetEnumParse_BuiltinShapeKind(s, tmp)) return false;
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
    extern const ClassMetaInfo Class_GeometryVertexFormatSpec;
    extern const ClassMetaInfo Class_GeometryBuiltinParamsSpec;
    extern const ClassMetaInfo Class_GeometryAssetSpec;

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

    inline const PropertyMetaInfo Meta_GeometryVertexFormatSpec[] =
    {
        { "normal", "normal", "normal", "Format", "法线顶点格式；接受短名 v2un8 / VF_ 宏名 / Vulkan 原名", "VertexNormalFormat", "hgl::graph::VertexNormalFormat", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 1u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VertexNormalFormat, &AssetEnumName64_VertexNormalFormat },
    };

    inline const ClassMetaInfo Class_GeometryVertexFormatSpec =
    {
        "GeometryVertexFormatSpec", "", "`[vertex_format]` 表：顶点法线格式覆盖（可选，整表缺省即用类内缺省值）。",
        Meta_GeometryVertexFormatSpec, (uint32_t)(sizeof(Meta_GeometryVertexFormatSpec) / sizeof(Meta_GeometryVertexFormatSpec[0])),
        1u, 1u
    };

    inline const PropertyMetaInfo Meta_GeometryBuiltinParamsSpec[] =
    {
        { "sphere", "sphere", "sphere", "Default", "", "BuiltinSphereParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 4u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinSphereParams, PropertyKind::None, nullptr, nullptr },
        { "dome", "dome", "dome", "Default", "", "BuiltinDomeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 4u, 4u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinDomeParams, PropertyKind::None, nullptr, nullptr },
        { "cube", "cube", "cube", "Default", "", "BuiltinCubeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 20u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinCubeParams, PropertyKind::None, nullptr, nullptr },
        { "plane_grid", "plane_grid", "plane_grid", "Default", "", "BuiltinPlaneGridParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 28u, 20u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinPlaneGridParams, PropertyKind::None, nullptr, nullptr },
        { "cylinder", "cylinder", "cylinder", "Default", "", "BuiltinCylinderParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 12u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinCylinderParams, PropertyKind::None, nullptr, nullptr },
        { "hollow_cylinder", "hollow_cylinder", "hollow_cylinder", "Default", "", "BuiltinHollowCylinderParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 60u, 24u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinHollowCylinderParams, PropertyKind::None, nullptr, nullptr },
        { "torus", "torus", "torus", "Default", "", "BuiltinTorusParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 84u, 24u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinTorusParams, PropertyKind::None, nullptr, nullptr },
        { "cone", "cone", "cone", "Default", "", "BuiltinConeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 108u, 16u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinConeParams, PropertyKind::None, nullptr, nullptr },
        { "capsule", "capsule", "capsule", "Default", "", "BuiltinCapsuleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 124u, 16u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinCapsuleParams, PropertyKind::None, nullptr, nullptr },
        { "rectangle", "rectangle", "rectangle", "Default", "", "BuiltinRectangleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 140u, 32u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinRectangleParams, PropertyKind::None, nullptr, nullptr },
        { "bounding_box", "bounding_box", "bounding_box", "Default", "", "BuiltinBoundingBoxParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 172u, 4u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinBoundingBoxParams, PropertyKind::None, nullptr, nullptr },
        { "hex_sphere", "hex_sphere", "hex_sphere", "Default", "", "BuiltinHexSphereParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 176u, 16u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinHexSphereParams, PropertyKind::None, nullptr, nullptr },
        { "tapered_capsule", "tapered_capsule", "tapered_capsule", "Default", "", "BuiltinTaperedCapsuleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 192u, 20u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinTaperedCapsuleParams, PropertyKind::None, nullptr, nullptr },
        { "frustum", "frustum", "frustum", "Default", "", "BuiltinFrustumParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 212u, 20u, 0u, 0.0, 0.0, 0.0, &Class_BuiltinFrustumParams, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_GeometryBuiltinParamsSpec =
    {
        "GeometryBuiltinParamsSpec", "", "`[builtin.<shape>]` 各形状参数子表。 `[builtin.sphere]` 这类**表名由 `shape` 字段的值决定**的写法，静态反射无法 表达；这里把 14 张子表展开成 14 个固定键（键名 = TOML 表名），由 `ReadFromValue` 一次读全 —— 出现几张贴就读几张，没写的表全是类内缺省值， 消费侧按 `shape` 直接取对应成员即可（不需要「先挑表再读」）。",
        Meta_GeometryBuiltinParamsSpec, (uint32_t)(sizeof(Meta_GeometryBuiltinParamsSpec) / sizeof(Meta_GeometryBuiltinParamsSpec[0])),
        232u, 4u
    };

    inline const PropertyMetaInfo Meta_GeometryAssetSpec[] =
    {
        { "schema", "schema", "schema", "Identity", "格式版本，当前只接受 1", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 8u, 0u, 1.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Identity", "资产名（缺省取文件名主干）", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "kind", "type", "kind", "Source", "builtin 参数化 / file 读盘", "GeometryAssetKind", "hgl::graph::GeometryAssetKind", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 1u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_GeometryAssetKind, &AssetEnumName64_GeometryAssetKind },
        { "shape", "shape", "shape", "Source", "仅 builtin：内置形状", "BuiltinShapeKind", "hgl::graph::BuiltinShapeKind", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 49u, 3u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_BuiltinShapeKind, &AssetEnumName64_BuiltinShapeKind },
        { "params", "builtin", "params", "Source", "仅 builtin：形状参数子表", "GeometryBuiltinParamsSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 52u, 236u, 0u, 0.0, 0.0, 0.0, &Class_GeometryBuiltinParamsSpec, PropertyKind::None, nullptr, nullptr },
        { "file_path", "file", "file_path", "Source", "仅 file：几何资产路径", "OSString", nullptr, "geo", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 288u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "vertex_format", "vertex_format", "vertex_format", "Format", "顶点格式覆盖（可选）", "GeometryVertexFormatSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 328u, 8u, 0u, 0.0, 0.0, 0.0, &Class_GeometryVertexFormatSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_GeometryAssetSpec =
    {
        "GeometryAssetSpec", "", "@brief 几何资产规约（`.geo.toml` / `.geo` 的唯一真源） TOML 形状： ```toml schema = 1 name   = \"unit_sphere\" type   = \"builtin\"    # 或 \"file\" shape  = \"sphere\"     # 仅 builtin file   = \"x.geometry\" # 仅 file [builtin.sphere] slices = 64 [vertex_format] normal = \"VF_V2UN8\" ``` 字段顺序即 TOML 键序；`kind` / `params` / `file_path` 的键名由 `Key` 覆盖。",
        Meta_GeometryAssetSpec, (uint32_t)(sizeof(Meta_GeometryAssetSpec) / sizeof(Meta_GeometryAssetSpec[0])),
        336u, 8u
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
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryVertexFormatSpec *) { return &Class_GeometryVertexFormatSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryBuiltinParamsSpec *) { return &Class_GeometryBuiltinParamsSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryAssetSpec *) { return &Class_GeometryAssetSpec; }
} // namespace hgl::asset::generated
