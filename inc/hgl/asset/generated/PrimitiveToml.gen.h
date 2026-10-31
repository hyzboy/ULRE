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
#include <hgl/asset/generated/PrimitiveSerialize.gen.h>

#include <hgl/graph/asset/PrimitiveAssetSpec.h>

namespace hgl::asset::generated
{
    inline bool ReadPrimitiveVariantSpec(const toml::value &table, hgl::graph::PrimitiveVariantSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveVariantSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WritePrimitiveVariantSpec(const hgl::graph::PrimitiveVariantSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveVariantSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

    inline bool ReadPrimitiveAssetSpec(const toml::value &table, hgl::graph::PrimitiveAssetSpec &out)
    {
        hgl::serialize::Value node;
        hgl::serialize::FromToml(table, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveAssetSpec>::Do(ar, out);
        return ar.Ok();
    }

    inline toml::value WritePrimitiveAssetSpec(const hgl::graph::PrimitiveAssetSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveAssetSpec>::Do(ar, in);
        return hgl::serialize::ToToml(node);
    }

}//namespace hgl::asset::generated
