// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   TOML 格式门面（Read* / Write*）。字段遍历仍然只有 Serialize.gen.h 里那一份，
//   本文件不含任何 FlatBuffers 依赖，数据面可以单独包含它。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/asset/generated/GeometrySerialize.gen.h>

#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::asset::generated
{
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

}//namespace hgl::asset::generated
