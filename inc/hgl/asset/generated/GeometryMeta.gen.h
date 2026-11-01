// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <cstddef>

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_NTBType(const hgl::graph::NTBType v)
    { return hgl::graph::GetNTBTypeName(v); }

    inline bool AssetEnumParse_NTBType(const char *s, hgl::graph::NTBType &out)
    { return hgl::graph::ParseNTBType(s, out); }

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
    extern const ClassMetaInfo Class_BuiltinAxisParams;
    extern const ClassMetaInfo Class_BuiltinBoundingBoxParams;
    extern const ClassMetaInfo Class_BuiltinHexSphereParams;
    extern const ClassMetaInfo Class_BuiltinTaperedCapsuleParams;
    extern const ClassMetaInfo Class_BuiltinFrustumParams;
    extern const ClassMetaInfo Class_GeometryVertexFormatSpec;
    extern const ClassMetaInfo Class_GeometryBuiltinParamsSpec;
    extern const ClassMetaInfo Class_GeometryAssetSpec;

    inline const PropertyMetaInfo Meta_BuiltinSphereParams[] =
    {
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinSphereParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinSphereParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinSphereParams =
    {
        "BuiltinSphereParams", "", "",
        Meta_BuiltinSphereParams, (uint32_t)(sizeof(Meta_BuiltinSphereParams) / sizeof(Meta_BuiltinSphereParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinSphereParams), (uint32_t)alignof(hgl::graph::BuiltinSphereParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinDomeParams[] =
    {
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinDomeParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinDomeParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinDomeParams =
    {
        "BuiltinDomeParams", "", "",
        Meta_BuiltinDomeParams, (uint32_t)(sizeof(Meta_BuiltinDomeParams) / sizeof(Meta_BuiltinDomeParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinDomeParams), (uint32_t)alignof(hgl::graph::BuiltinDomeParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinCubeParams[] =
    {
        { "segments_x", "segments_x", "segments_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCubeParams, segments_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCubeParams::segments_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_y", "segments_y", "segments_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCubeParams, segments_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCubeParams::segments_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_z", "segments_z", "segments_z", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCubeParams, segments_z), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCubeParams::segments_z)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCubeParams, ntb), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCubeParams::ntb)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
        { "tex_coord", "tex_coord", "tex_coord", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCubeParams, tex_coord), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCubeParams::tex_coord)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCubeParams =
    {
        "BuiltinCubeParams", "", "",
        Meta_BuiltinCubeParams, (uint32_t)(sizeof(Meta_BuiltinCubeParams) / sizeof(Meta_BuiltinCubeParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinCubeParams), (uint32_t)alignof(hgl::graph::BuiltinCubeParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinPlaneGridParams[] =
    {
        { "grid_x", "grid_x", "grid_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, grid_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::grid_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "grid_y", "grid_y", "grid_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, grid_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::grid_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_x", "sub_x", "sub_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, sub_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::sub_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_y", "sub_y", "sub_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, sub_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::sub_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "lum", "lum", "lum", "Default", "", "uint8_t", nullptr, nullptr, PropertyKind::UInt8, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, lum), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::lum)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "sub_lum", "sub_lum", "sub_lum", "Default", "", "uint8_t", nullptr, nullptr, PropertyKind::UInt8, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, sub_lum), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::sub_lum)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "is_3d", "is_3d", "is_3d", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinPlaneGridParams, is_3d), (uint32_t)sizeof(decltype(hgl::graph::BuiltinPlaneGridParams::is_3d)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinPlaneGridParams =
    {
        "BuiltinPlaneGridParams", "", "",
        Meta_BuiltinPlaneGridParams, (uint32_t)(sizeof(Meta_BuiltinPlaneGridParams) / sizeof(Meta_BuiltinPlaneGridParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinPlaneGridParams), (uint32_t)alignof(hgl::graph::BuiltinPlaneGridParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinCylinderParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCylinderParams, half_extend), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCylinderParams::half_extend)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCylinderParams, radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCylinderParams::radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCylinderParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCylinderParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCylinderParams =
    {
        "BuiltinCylinderParams", "", "",
        Meta_BuiltinCylinderParams, (uint32_t)(sizeof(Meta_BuiltinCylinderParams) / sizeof(Meta_BuiltinCylinderParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinCylinderParams), (uint32_t)alignof(hgl::graph::BuiltinCylinderParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinHollowCylinderParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, half_extend), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::half_extend)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "inner_radius", "inner_radius", "inner_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, inner_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::inner_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "outer_radius", "outer_radius", "outer_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, outer_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::outer_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "cap_radial_tiles", "cap_radial_tiles", "cap_radial_tiles", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, cap_radial_tiles), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::cap_radial_tiles)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "cap_angular_tiles", "cap_angular_tiles", "cap_angular_tiles", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHollowCylinderParams, cap_angular_tiles), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHollowCylinderParams::cap_angular_tiles)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinHollowCylinderParams =
    {
        "BuiltinHollowCylinderParams", "", "",
        Meta_BuiltinHollowCylinderParams, (uint32_t)(sizeof(Meta_BuiltinHollowCylinderParams) / sizeof(Meta_BuiltinHollowCylinderParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinHollowCylinderParams), (uint32_t)alignof(hgl::graph::BuiltinHollowCylinderParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinTorusParams[] =
    {
        { "inner_radius", "inner_radius", "inner_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, inner_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::inner_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "outer_radius", "outer_radius", "outer_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, outer_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::outer_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, stacks), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::stacks)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_x", "uv_scale_x", "uv_scale_x", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, uv_scale_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::uv_scale_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_y", "uv_scale_y", "uv_scale_y", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTorusParams, uv_scale_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTorusParams::uv_scale_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinTorusParams =
    {
        "BuiltinTorusParams", "", "",
        Meta_BuiltinTorusParams, (uint32_t)(sizeof(Meta_BuiltinTorusParams) / sizeof(Meta_BuiltinTorusParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinTorusParams), (uint32_t)alignof(hgl::graph::BuiltinTorusParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinConeParams[] =
    {
        { "half_extend", "half_extend", "half_extend", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinConeParams, half_extend), (uint32_t)sizeof(decltype(hgl::graph::BuiltinConeParams::half_extend)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinConeParams, radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinConeParams::radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinConeParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinConeParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinConeParams, stacks), (uint32_t)sizeof(decltype(hgl::graph::BuiltinConeParams::stacks)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinConeParams =
    {
        "BuiltinConeParams", "", "",
        Meta_BuiltinConeParams, (uint32_t)(sizeof(Meta_BuiltinConeParams) / sizeof(Meta_BuiltinConeParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinConeParams), (uint32_t)alignof(hgl::graph::BuiltinConeParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinCapsuleParams[] =
    {
        { "half_height", "half_height", "half_height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCapsuleParams, half_height), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCapsuleParams::half_height)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCapsuleParams, radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCapsuleParams::radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCapsuleParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCapsuleParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinCapsuleParams, stacks), (uint32_t)sizeof(decltype(hgl::graph::BuiltinCapsuleParams::stacks)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinCapsuleParams =
    {
        "BuiltinCapsuleParams", "", "",
        Meta_BuiltinCapsuleParams, (uint32_t)(sizeof(Meta_BuiltinCapsuleParams) / sizeof(Meta_BuiltinCapsuleParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinCapsuleParams), (uint32_t)alignof(hgl::graph::BuiltinCapsuleParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinRectangleParams[] =
    {
        { "left", "left", "left", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, left), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::left)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top", "top", "top", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, top), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::top)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "width", "width", "width", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, width), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::width)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "height", "height", "height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, height), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::height)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_x", "segments_x", "segments_x", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, segments_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::segments_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "segments_y", "segments_y", "segments_y", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, segments_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::segments_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, ntb), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::ntb)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
        { "tex_coord", "tex_coord", "tex_coord", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinRectangleParams, tex_coord), (uint32_t)sizeof(decltype(hgl::graph::BuiltinRectangleParams::tex_coord)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinRectangleParams =
    {
        "BuiltinRectangleParams", "", "",
        Meta_BuiltinRectangleParams, (uint32_t)(sizeof(Meta_BuiltinRectangleParams) / sizeof(Meta_BuiltinRectangleParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinRectangleParams), (uint32_t)alignof(hgl::graph::BuiltinRectangleParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinAxisParams[] =
    {
        { "size", "size", "size", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinAxisParams, size), (uint32_t)sizeof(decltype(hgl::graph::BuiltinAxisParams::size)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinAxisParams =
    {
        "BuiltinAxisParams", "", "",
        Meta_BuiltinAxisParams, (uint32_t)(sizeof(Meta_BuiltinAxisParams) / sizeof(Meta_BuiltinAxisParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinAxisParams), (uint32_t)alignof(hgl::graph::BuiltinAxisParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinBoundingBoxParams[] =
    {
        { "ntb", "ntb", "ntb", "Default", "", "NTBType", "hgl::graph::NTBType", nullptr, PropertyKind::Enum, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinBoundingBoxParams, ntb), (uint32_t)sizeof(decltype(hgl::graph::BuiltinBoundingBoxParams::ntb)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_NTBType, &AssetEnumName64_NTBType },
    };

    inline const ClassMetaInfo Class_BuiltinBoundingBoxParams =
    {
        "BuiltinBoundingBoxParams", "", "",
        Meta_BuiltinBoundingBoxParams, (uint32_t)(sizeof(Meta_BuiltinBoundingBoxParams) / sizeof(Meta_BuiltinBoundingBoxParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinBoundingBoxParams), (uint32_t)alignof(hgl::graph::BuiltinBoundingBoxParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinHexSphereParams[] =
    {
        { "subdivisions", "subdivisions", "subdivisions", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHexSphereParams, subdivisions), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHexSphereParams::subdivisions)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "radius", "radius", "radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHexSphereParams, radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHexSphereParams::radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_x", "uv_scale_x", "uv_scale_x", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHexSphereParams, uv_scale_x), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHexSphereParams::uv_scale_x)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "uv_scale_y", "uv_scale_y", "uv_scale_y", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinHexSphereParams, uv_scale_y), (uint32_t)sizeof(decltype(hgl::graph::BuiltinHexSphereParams::uv_scale_y)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinHexSphereParams =
    {
        "BuiltinHexSphereParams", "", "",
        Meta_BuiltinHexSphereParams, (uint32_t)(sizeof(Meta_BuiltinHexSphereParams) / sizeof(Meta_BuiltinHexSphereParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinHexSphereParams), (uint32_t)alignof(hgl::graph::BuiltinHexSphereParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinTaperedCapsuleParams[] =
    {
        { "half_height", "half_height", "half_height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTaperedCapsuleParams, half_height), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTaperedCapsuleParams::half_height)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "bottom_radius", "bottom_radius", "bottom_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTaperedCapsuleParams, bottom_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTaperedCapsuleParams::bottom_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top_radius", "top_radius", "top_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTaperedCapsuleParams, top_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTaperedCapsuleParams::top_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTaperedCapsuleParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTaperedCapsuleParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "stacks", "stacks", "stacks", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinTaperedCapsuleParams, stacks), (uint32_t)sizeof(decltype(hgl::graph::BuiltinTaperedCapsuleParams::stacks)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinTaperedCapsuleParams =
    {
        "BuiltinTaperedCapsuleParams", "", "",
        Meta_BuiltinTaperedCapsuleParams, (uint32_t)(sizeof(Meta_BuiltinTaperedCapsuleParams) / sizeof(Meta_BuiltinTaperedCapsuleParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinTaperedCapsuleParams), (uint32_t)alignof(hgl::graph::BuiltinTaperedCapsuleParams)
    };

    inline const PropertyMetaInfo Meta_BuiltinFrustumParams[] =
    {
        { "bottom_radius", "bottom_radius", "bottom_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinFrustumParams, bottom_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinFrustumParams::bottom_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "top_radius", "top_radius", "top_radius", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinFrustumParams, top_radius), (uint32_t)sizeof(decltype(hgl::graph::BuiltinFrustumParams::top_radius)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "height", "height", "height", "Default", "", "float", nullptr, nullptr, PropertyKind::Float, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinFrustumParams, height), (uint32_t)sizeof(decltype(hgl::graph::BuiltinFrustumParams::height)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "slices", "slices", "slices", "Default", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinFrustumParams, slices), (uint32_t)sizeof(decltype(hgl::graph::BuiltinFrustumParams::slices)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "generate_caps", "generate_caps", "generate_caps", "Default", "", "bool", nullptr, nullptr, PropertyKind::Bool, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::BuiltinFrustumParams, generate_caps), (uint32_t)sizeof(decltype(hgl::graph::BuiltinFrustumParams::generate_caps)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_BuiltinFrustumParams =
    {
        "BuiltinFrustumParams", "", "",
        Meta_BuiltinFrustumParams, (uint32_t)(sizeof(Meta_BuiltinFrustumParams) / sizeof(Meta_BuiltinFrustumParams[0])),
        (uint32_t)sizeof(hgl::graph::BuiltinFrustumParams), (uint32_t)alignof(hgl::graph::BuiltinFrustumParams)
    };

    inline const PropertyMetaInfo Meta_GeometryVertexFormatSpec[] =
    {
        { "normal", "normal", "normal", "Format", "法线顶点格式；接受短名 v2un8 / VF_ 宏名 / Vulkan 原名", "VertexNormalFormat", "hgl::graph::VertexNormalFormat", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryVertexFormatSpec, normal), (uint32_t)sizeof(decltype(hgl::graph::GeometryVertexFormatSpec::normal)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_VertexNormalFormat, &AssetEnumName64_VertexNormalFormat },
    };

    inline const ClassMetaInfo Class_GeometryVertexFormatSpec =
    {
        "GeometryVertexFormatSpec", "", "`[vertex_format]` 表：顶点法线格式覆盖（可选，整表缺省即用类内缺省值）。",
        Meta_GeometryVertexFormatSpec, (uint32_t)(sizeof(Meta_GeometryVertexFormatSpec) / sizeof(Meta_GeometryVertexFormatSpec[0])),
        (uint32_t)sizeof(hgl::graph::GeometryVertexFormatSpec), (uint32_t)alignof(hgl::graph::GeometryVertexFormatSpec)
    };

    inline const PropertyMetaInfo Meta_GeometryBuiltinParamsSpec[] =
    {
        { "sphere", "sphere", "sphere", "Default", "", "BuiltinSphereParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, sphere), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::sphere)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinSphereParams, PropertyKind::None, nullptr, nullptr },
        { "dome", "dome", "dome", "Default", "", "BuiltinDomeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, dome), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::dome)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinDomeParams, PropertyKind::None, nullptr, nullptr },
        { "cube", "cube", "cube", "Default", "", "BuiltinCubeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, cube), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::cube)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinCubeParams, PropertyKind::None, nullptr, nullptr },
        { "plane_grid", "plane_grid", "plane_grid", "Default", "", "BuiltinPlaneGridParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, plane_grid), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::plane_grid)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinPlaneGridParams, PropertyKind::None, nullptr, nullptr },
        { "cylinder", "cylinder", "cylinder", "Default", "", "BuiltinCylinderParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, cylinder), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::cylinder)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinCylinderParams, PropertyKind::None, nullptr, nullptr },
        { "hollow_cylinder", "hollow_cylinder", "hollow_cylinder", "Default", "", "BuiltinHollowCylinderParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, hollow_cylinder), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::hollow_cylinder)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinHollowCylinderParams, PropertyKind::None, nullptr, nullptr },
        { "torus", "torus", "torus", "Default", "", "BuiltinTorusParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, torus), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::torus)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinTorusParams, PropertyKind::None, nullptr, nullptr },
        { "cone", "cone", "cone", "Default", "", "BuiltinConeParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, cone), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::cone)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinConeParams, PropertyKind::None, nullptr, nullptr },
        { "capsule", "capsule", "capsule", "Default", "", "BuiltinCapsuleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, capsule), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::capsule)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinCapsuleParams, PropertyKind::None, nullptr, nullptr },
        { "rectangle", "rectangle", "rectangle", "Default", "", "BuiltinRectangleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, rectangle), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::rectangle)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinRectangleParams, PropertyKind::None, nullptr, nullptr },
        { "axis", "axis", "axis", "Default", "", "BuiltinAxisParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, axis), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::axis)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinAxisParams, PropertyKind::None, nullptr, nullptr },
        { "bounding_box", "bounding_box", "bounding_box", "Default", "", "BuiltinBoundingBoxParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, bounding_box), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::bounding_box)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinBoundingBoxParams, PropertyKind::None, nullptr, nullptr },
        { "hex_sphere", "hex_sphere", "hex_sphere", "Default", "", "BuiltinHexSphereParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, hex_sphere), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::hex_sphere)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinHexSphereParams, PropertyKind::None, nullptr, nullptr },
        { "tapered_capsule", "tapered_capsule", "tapered_capsule", "Default", "", "BuiltinTaperedCapsuleParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, tapered_capsule), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::tapered_capsule)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinTaperedCapsuleParams, PropertyKind::None, nullptr, nullptr },
        { "frustum", "frustum", "frustum", "Default", "", "BuiltinFrustumParams", nullptr, nullptr, PropertyKind::Struct, PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryBuiltinParamsSpec, frustum), (uint32_t)sizeof(decltype(hgl::graph::GeometryBuiltinParamsSpec::frustum)), 0u, 0.0, 0.0, 0.0, &Class_BuiltinFrustumParams, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_GeometryBuiltinParamsSpec =
    {
        "GeometryBuiltinParamsSpec", "", "`[builtin.<shape>]` 各形状参数子表。 `[builtin.sphere]` 这类**表名由 `shape` 字段的值决定**的写法，静态反射无法 表达；这里把 14 张子表展开成 14 个固定键（键名 = TOML 表名），由 `ReadFromValue` 一次读全 —— 出现几张贴就读几张，没写的表全是类内缺省值， 消费侧按 `shape` 直接取对应成员即可（不需要「先挑表再读」）。",
        Meta_GeometryBuiltinParamsSpec, (uint32_t)(sizeof(Meta_GeometryBuiltinParamsSpec) / sizeof(Meta_GeometryBuiltinParamsSpec[0])),
        (uint32_t)sizeof(hgl::graph::GeometryBuiltinParamsSpec), (uint32_t)alignof(hgl::graph::GeometryBuiltinParamsSpec)
    };

    inline const PropertyMetaInfo Meta_GeometryAssetSpec[] =
    {
        { "schema", "schema", "schema", "Identity", "格式版本，当前只接受 1", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, schema), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::schema)), 0u, 1.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Identity", "资产名（缺省取文件名主干）", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, name), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::name)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "kind", "type", "kind", "Source", "builtin 参数化 / file 读盘", "GeometryAssetKind", "hgl::graph::GeometryAssetKind", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, kind), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::kind)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_GeometryAssetKind, &AssetEnumName64_GeometryAssetKind },
        { "shape", "shape", "shape", "Source", "仅 builtin：内置形状", "BuiltinShapeKind", "hgl::graph::BuiltinShapeKind", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, shape), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::shape)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_BuiltinShapeKind, &AssetEnumName64_BuiltinShapeKind },
        { "params", "builtin", "params", "Source", "仅 builtin：形状参数子表", "GeometryBuiltinParamsSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, params), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::params)), 0u, 0.0, 0.0, 0.0, &Class_GeometryBuiltinParamsSpec, PropertyKind::None, nullptr, nullptr },
        { "file_path", "file", "file_path", "Source", "仅 file：几何资产路径", "OSString", nullptr, "geo", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, file_path), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::file_path)), 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "vertex_format", "vertex_format", "vertex_format", "Format", "顶点格式覆盖（可选）", "GeometryVertexFormatSpec", nullptr, nullptr, PropertyKind::Struct, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, (uint32_t)offsetof(hgl::graph::GeometryAssetSpec, vertex_format), (uint32_t)sizeof(decltype(hgl::graph::GeometryAssetSpec::vertex_format)), 0u, 0.0, 0.0, 0.0, &Class_GeometryVertexFormatSpec, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_GeometryAssetSpec =
    {
        "GeometryAssetSpec", "", "@brief 几何资产规约（`.geo.toml` / `.geo` 的唯一真源） TOML 形状： ```toml schema = 1 name   = \"unit_sphere\" type   = \"builtin\"    # 或 \"file\" shape  = \"sphere\"     # 仅 builtin file   = \"x.geometry\" # 仅 file [builtin.sphere] slices = 64 [vertex_format] normal = \"VF_V2UN8\" ``` 字段顺序即 TOML 键序；`kind` / `params` / `file_path` 的键名由 `Key` 覆盖。",
        Meta_GeometryAssetSpec, (uint32_t)(sizeof(Meta_GeometryAssetSpec) / sizeof(Meta_GeometryAssetSpec[0])),
        (uint32_t)sizeof(hgl::graph::GeometryAssetSpec), (uint32_t)alignof(hgl::graph::GeometryAssetSpec)
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
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinAxisParams *) { return &Class_BuiltinAxisParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinBoundingBoxParams *) { return &Class_BuiltinBoundingBoxParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinHexSphereParams *) { return &Class_BuiltinHexSphereParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinTaperedCapsuleParams *) { return &Class_BuiltinTaperedCapsuleParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::BuiltinFrustumParams *) { return &Class_BuiltinFrustumParams; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryVertexFormatSpec *) { return &Class_GeometryVertexFormatSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryBuiltinParamsSpec *) { return &Class_GeometryBuiltinParamsSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::GeometryAssetSpec *) { return &Class_GeometryAssetSpec; }
} // namespace hgl::asset::generated
