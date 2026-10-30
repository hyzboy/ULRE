// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这里是**布局描述**（FlatBuffers 的 voffset / 内联 struct / 向量元素类型）
//   与格式门面。字段遍历仍然只有 Serialize.gen.h 里那一份。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/asset/generated/GeometrySerialize.gen.h>
#include <hgl/asset/generated/geometry_schema_generated.h>
#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
    namespace fb = hgl::serialize::fb;

    inline bool NTBType_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::NTBType v{};
        if (!hgl::asset::generated::AssetEnumParse_NTBType(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *NTBType_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_NTBType(static_cast<hgl::graph::NTBType>(v));
    }

    inline const fb::FieldDesc BuiltinSphereParams_fbs_fields[] = {
        {1, "slices", fb::FbType::UInt32, 4},
    };
    inline const fb::TableDesc BuiltinSphereParams_fbs = {"BuiltinSphereParams", BuiltinSphereParams_fbs_fields, 1};

    inline const fb::FieldDesc BuiltinDomeParams_fbs_fields[] = {
        {1, "slices", fb::FbType::UInt32, 4},
    };
    inline const fb::TableDesc BuiltinDomeParams_fbs = {"BuiltinDomeParams", BuiltinDomeParams_fbs_fields, 1};

    inline const fb::FieldDesc BuiltinCubeParams_fbs_fields[] = {
        {1, "segments_x", fb::FbType::UInt32, 4},
        {2, "segments_y", fb::FbType::UInt32, 6},
        {3, "segments_z", fb::FbType::UInt32, 8},
        {4, "ntb", fb::FbType::Enum, 10, fb::FbType::None, 0, nullptr, &NTBType_FbsFromName, &NTBType_FbsToName},
        {5, "tex_coord", fb::FbType::Bool, 12},
    };
    inline const fb::TableDesc BuiltinCubeParams_fbs = {"BuiltinCubeParams", BuiltinCubeParams_fbs_fields, 5};

    inline const fb::FieldDesc BuiltinPlaneGridParams_fbs_fields[] = {
        {1, "grid_x", fb::FbType::UInt32, 4},
        {2, "grid_y", fb::FbType::UInt32, 6},
        {3, "sub_x", fb::FbType::UInt32, 8},
        {4, "sub_y", fb::FbType::UInt32, 10},
        {5, "lum", fb::FbType::UInt8, 12},
        {6, "sub_lum", fb::FbType::UInt8, 14},
        {7, "is_3d", fb::FbType::Bool, 16},
    };
    inline const fb::TableDesc BuiltinPlaneGridParams_fbs = {"BuiltinPlaneGridParams", BuiltinPlaneGridParams_fbs_fields, 7};

    inline const fb::FieldDesc BuiltinCylinderParams_fbs_fields[] = {
        {1, "half_extend", fb::FbType::Float, 4},
        {2, "radius", fb::FbType::Float, 6},
        {3, "slices", fb::FbType::UInt32, 8},
    };
    inline const fb::TableDesc BuiltinCylinderParams_fbs = {"BuiltinCylinderParams", BuiltinCylinderParams_fbs_fields, 3};

    inline const fb::FieldDesc BuiltinHollowCylinderParams_fbs_fields[] = {
        {1, "half_extend", fb::FbType::Float, 4},
        {2, "inner_radius", fb::FbType::Float, 6},
        {3, "outer_radius", fb::FbType::Float, 8},
        {4, "slices", fb::FbType::UInt32, 10},
        {5, "cap_radial_tiles", fb::FbType::Float, 12},
        {6, "cap_angular_tiles", fb::FbType::Float, 14},
    };
    inline const fb::TableDesc BuiltinHollowCylinderParams_fbs = {"BuiltinHollowCylinderParams", BuiltinHollowCylinderParams_fbs_fields, 6};

    inline const fb::FieldDesc BuiltinTorusParams_fbs_fields[] = {
        {1, "inner_radius", fb::FbType::Float, 4},
        {2, "outer_radius", fb::FbType::Float, 6},
        {3, "slices", fb::FbType::UInt32, 8},
        {4, "stacks", fb::FbType::UInt32, 10},
        {5, "uv_scale_x", fb::FbType::Float, 12},
        {6, "uv_scale_y", fb::FbType::Float, 14},
    };
    inline const fb::TableDesc BuiltinTorusParams_fbs = {"BuiltinTorusParams", BuiltinTorusParams_fbs_fields, 6};

    inline const fb::FieldDesc BuiltinConeParams_fbs_fields[] = {
        {1, "half_extend", fb::FbType::Float, 4},
        {2, "radius", fb::FbType::Float, 6},
        {3, "slices", fb::FbType::UInt32, 8},
        {4, "stacks", fb::FbType::UInt32, 10},
    };
    inline const fb::TableDesc BuiltinConeParams_fbs = {"BuiltinConeParams", BuiltinConeParams_fbs_fields, 4};

    inline const fb::FieldDesc BuiltinCapsuleParams_fbs_fields[] = {
        {1, "half_height", fb::FbType::Float, 4},
        {2, "radius", fb::FbType::Float, 6},
        {3, "slices", fb::FbType::UInt32, 8},
        {4, "stacks", fb::FbType::UInt32, 10},
    };
    inline const fb::TableDesc BuiltinCapsuleParams_fbs = {"BuiltinCapsuleParams", BuiltinCapsuleParams_fbs_fields, 4};

    inline const fb::FieldDesc BuiltinRectangleParams_fbs_fields[] = {
        {1, "left", fb::FbType::Float, 4},
        {2, "top", fb::FbType::Float, 6},
        {3, "width", fb::FbType::Float, 8},
        {4, "height", fb::FbType::Float, 10},
        {5, "segments_x", fb::FbType::UInt32, 12},
        {6, "segments_y", fb::FbType::UInt32, 14},
        {7, "ntb", fb::FbType::Enum, 16, fb::FbType::None, 0, nullptr, &NTBType_FbsFromName, &NTBType_FbsToName},
        {8, "tex_coord", fb::FbType::Bool, 18},
    };
    inline const fb::TableDesc BuiltinRectangleParams_fbs = {"BuiltinRectangleParams", BuiltinRectangleParams_fbs_fields, 8};

    inline const fb::FieldDesc BuiltinBoundingBoxParams_fbs_fields[] = {
        {1, "ntb", fb::FbType::Enum, 4, fb::FbType::None, 0, nullptr, &NTBType_FbsFromName, &NTBType_FbsToName},
    };
    inline const fb::TableDesc BuiltinBoundingBoxParams_fbs = {"BuiltinBoundingBoxParams", BuiltinBoundingBoxParams_fbs_fields, 1};

    inline const fb::FieldDesc BuiltinHexSphereParams_fbs_fields[] = {
        {1, "subdivisions", fb::FbType::UInt32, 4},
        {2, "radius", fb::FbType::Float, 6},
        {3, "uv_scale_x", fb::FbType::Float, 8},
        {4, "uv_scale_y", fb::FbType::Float, 10},
    };
    inline const fb::TableDesc BuiltinHexSphereParams_fbs = {"BuiltinHexSphereParams", BuiltinHexSphereParams_fbs_fields, 4};

    inline const fb::FieldDesc BuiltinTaperedCapsuleParams_fbs_fields[] = {
        {1, "half_height", fb::FbType::Float, 4},
        {2, "bottom_radius", fb::FbType::Float, 6},
        {3, "top_radius", fb::FbType::Float, 8},
        {4, "slices", fb::FbType::UInt32, 10},
        {5, "stacks", fb::FbType::UInt32, 12},
    };
    inline const fb::TableDesc BuiltinTaperedCapsuleParams_fbs = {"BuiltinTaperedCapsuleParams", BuiltinTaperedCapsuleParams_fbs_fields, 5};

    inline const fb::FieldDesc BuiltinFrustumParams_fbs_fields[] = {
        {1, "bottom_radius", fb::FbType::Float, 4},
        {2, "top_radius", fb::FbType::Float, 6},
        {3, "height", fb::FbType::Float, 8},
        {4, "slices", fb::FbType::UInt32, 10},
        {5, "generate_caps", fb::FbType::Bool, 12},
    };
    inline const fb::TableDesc BuiltinFrustumParams_fbs = {"BuiltinFrustumParams", BuiltinFrustumParams_fbs_fields, 5};

    inline bool ReadBuiltinSphereParams(const toml::value &table, hgl::graph::BuiltinSphereParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinSphereParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinSphereParams(const hgl::graph::BuiltinSphereParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinSphereParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinDomeParams(const toml::value &table, hgl::graph::BuiltinDomeParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinDomeParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinDomeParams(const hgl::graph::BuiltinDomeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinDomeParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinCubeParams(const toml::value &table, hgl::graph::BuiltinCubeParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCubeParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinCubeParams(const hgl::graph::BuiltinCubeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCubeParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinPlaneGridParams(const toml::value &table, hgl::graph::BuiltinPlaneGridParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinPlaneGridParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinPlaneGridParams(const hgl::graph::BuiltinPlaneGridParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinPlaneGridParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinCylinderParams(const toml::value &table, hgl::graph::BuiltinCylinderParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCylinderParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinCylinderParams(const hgl::graph::BuiltinCylinderParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCylinderParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinHollowCylinderParams(const toml::value &table, hgl::graph::BuiltinHollowCylinderParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHollowCylinderParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinHollowCylinderParams(const hgl::graph::BuiltinHollowCylinderParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHollowCylinderParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinTorusParams(const toml::value &table, hgl::graph::BuiltinTorusParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTorusParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinTorusParams(const hgl::graph::BuiltinTorusParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTorusParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinConeParams(const toml::value &table, hgl::graph::BuiltinConeParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinConeParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinConeParams(const hgl::graph::BuiltinConeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinConeParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinCapsuleParams(const toml::value &table, hgl::graph::BuiltinCapsuleParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCapsuleParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinCapsuleParams(const hgl::graph::BuiltinCapsuleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCapsuleParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinRectangleParams(const toml::value &table, hgl::graph::BuiltinRectangleParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinRectangleParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinRectangleParams(const hgl::graph::BuiltinRectangleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinRectangleParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinBoundingBoxParams(const toml::value &table, hgl::graph::BuiltinBoundingBoxParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinBoundingBoxParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinBoundingBoxParams(const hgl::graph::BuiltinBoundingBoxParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinBoundingBoxParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinHexSphereParams(const toml::value &table, hgl::graph::BuiltinHexSphereParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHexSphereParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinHexSphereParams(const hgl::graph::BuiltinHexSphereParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHexSphereParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinTaperedCapsuleParams(const toml::value &table, hgl::graph::BuiltinTaperedCapsuleParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTaperedCapsuleParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinTaperedCapsuleParams(const hgl::graph::BuiltinTaperedCapsuleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTaperedCapsuleParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadBuiltinFrustumParams(const toml::value &table, hgl::graph::BuiltinFrustumParams &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinFrustumParams>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WriteBuiltinFrustumParams(const hgl::graph::BuiltinFrustumParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinFrustumParams>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinSphereParams>
    EncodeBuiltinSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinSphereParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinSphereParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinSphereParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinSphereParams_fbs).o);
    }

    inline void DecodeBuiltinSphereParams(const ::hgl::asset::schema::BuiltinSphereParams *root, hgl::graph::BuiltinSphereParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinSphereParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinSphereParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinDomeParams>
    EncodeBuiltinDomeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinDomeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinDomeParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinDomeParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinDomeParams_fbs).o);
    }

    inline void DecodeBuiltinDomeParams(const ::hgl::asset::schema::BuiltinDomeParams *root, hgl::graph::BuiltinDomeParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinDomeParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinDomeParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCubeParams>
    EncodeBuiltinCubeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCubeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCubeParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinCubeParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinCubeParams_fbs).o);
    }

    inline void DecodeBuiltinCubeParams(const ::hgl::asset::schema::BuiltinCubeParams *root, hgl::graph::BuiltinCubeParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinCubeParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCubeParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinPlaneGridParams>
    EncodeBuiltinPlaneGridParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinPlaneGridParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinPlaneGridParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinPlaneGridParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinPlaneGridParams_fbs).o);
    }

    inline void DecodeBuiltinPlaneGridParams(const ::hgl::asset::schema::BuiltinPlaneGridParams *root, hgl::graph::BuiltinPlaneGridParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinPlaneGridParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinPlaneGridParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCylinderParams>
    EncodeBuiltinCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCylinderParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCylinderParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinCylinderParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinCylinderParams_fbs).o);
    }

    inline void DecodeBuiltinCylinderParams(const ::hgl::asset::schema::BuiltinCylinderParams *root, hgl::graph::BuiltinCylinderParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinCylinderParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCylinderParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHollowCylinderParams>
    EncodeBuiltinHollowCylinderParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHollowCylinderParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHollowCylinderParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinHollowCylinderParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinHollowCylinderParams_fbs).o);
    }

    inline void DecodeBuiltinHollowCylinderParams(const ::hgl::asset::schema::BuiltinHollowCylinderParams *root, hgl::graph::BuiltinHollowCylinderParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinHollowCylinderParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHollowCylinderParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTorusParams>
    EncodeBuiltinTorusParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTorusParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTorusParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinTorusParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinTorusParams_fbs).o);
    }

    inline void DecodeBuiltinTorusParams(const ::hgl::asset::schema::BuiltinTorusParams *root, hgl::graph::BuiltinTorusParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinTorusParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTorusParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinConeParams>
    EncodeBuiltinConeParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinConeParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinConeParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinConeParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinConeParams_fbs).o);
    }

    inline void DecodeBuiltinConeParams(const ::hgl::asset::schema::BuiltinConeParams *root, hgl::graph::BuiltinConeParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinConeParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinConeParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinCapsuleParams>
    EncodeBuiltinCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinCapsuleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCapsuleParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinCapsuleParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinCapsuleParams_fbs).o);
    }

    inline void DecodeBuiltinCapsuleParams(const ::hgl::asset::schema::BuiltinCapsuleParams *root, hgl::graph::BuiltinCapsuleParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinCapsuleParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinCapsuleParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinRectangleParams>
    EncodeBuiltinRectangleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinRectangleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinRectangleParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinRectangleParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinRectangleParams_fbs).o);
    }

    inline void DecodeBuiltinRectangleParams(const ::hgl::asset::schema::BuiltinRectangleParams *root, hgl::graph::BuiltinRectangleParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinRectangleParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinRectangleParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinBoundingBoxParams>
    EncodeBuiltinBoundingBoxParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinBoundingBoxParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinBoundingBoxParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinBoundingBoxParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinBoundingBoxParams_fbs).o);
    }

    inline void DecodeBuiltinBoundingBoxParams(const ::hgl::asset::schema::BuiltinBoundingBoxParams *root, hgl::graph::BuiltinBoundingBoxParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinBoundingBoxParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinBoundingBoxParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinHexSphereParams>
    EncodeBuiltinHexSphereParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinHexSphereParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHexSphereParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinHexSphereParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinHexSphereParams_fbs).o);
    }

    inline void DecodeBuiltinHexSphereParams(const ::hgl::asset::schema::BuiltinHexSphereParams *root, hgl::graph::BuiltinHexSphereParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinHexSphereParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinHexSphereParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinTaperedCapsuleParams>
    EncodeBuiltinTaperedCapsuleParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinTaperedCapsuleParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTaperedCapsuleParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinTaperedCapsuleParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinTaperedCapsuleParams_fbs).o);
    }

    inline void DecodeBuiltinTaperedCapsuleParams(const ::hgl::asset::schema::BuiltinTaperedCapsuleParams *root, hgl::graph::BuiltinTaperedCapsuleParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinTaperedCapsuleParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinTaperedCapsuleParams>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::BuiltinFrustumParams>
    EncodeBuiltinFrustumParams(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::BuiltinFrustumParams &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinFrustumParams>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::BuiltinFrustumParams>(
            hgl::serialize::fb::EncodeTable(fbb, node, BuiltinFrustumParams_fbs).o);
    }

    inline void DecodeBuiltinFrustumParams(const ::hgl::asset::schema::BuiltinFrustumParams *root, hgl::graph::BuiltinFrustumParams &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            BuiltinFrustumParams_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::BuiltinFrustumParams>::Do(ar, out);
    }

}//namespace hgl::asset::generated
