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
#include <hgl/asset/generated/PrimitiveSerialize.gen.h>
#include <hgl/asset/generated/primitive_schema_generated.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>

namespace hgl::asset::generated
{
    namespace fb = hgl::serialize::fb;

    inline bool PrimitiveVariantPurpose_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::PrimitiveVariantPurpose v{};
        if (!hgl::asset::generated::AssetEnumParse_PrimitiveVariantPurpose(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *PrimitiveVariantPurpose_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_PrimitiveVariantPurpose(static_cast<hgl::graph::PrimitiveVariantPurpose>(v));
    }

    inline bool PrimitiveType_FbsFromName(const char *s, int64_t &out)
    {
        hgl::graph::PrimitiveType v{};
        if (!hgl::asset::generated::AssetEnumParse_PrimitiveType(s, v)) return false;
        out = static_cast<int64_t>(v);
        return true;
    }

    inline const char *PrimitiveType_FbsToName(const int64_t v)
    {
        return hgl::asset::generated::AssetEnumName_PrimitiveType(static_cast<hgl::graph::PrimitiveType>(v));
    }

    inline const fb::FieldDesc PrimitiveVariantSpec_fbs_fields[] = {
        {1, "recipe", fb::FbType::String, 4},
        {2, "purpose", fb::FbType::Enum, 6, fb::FbType::None, 0, nullptr, &PrimitiveVariantPurpose_FbsFromName, &PrimitiveVariantPurpose_FbsToName},
        {3, "lod", fb::FbType::UInt16, 8},
    };
    inline const fb::TableDesc PrimitiveVariantSpec_fbs = {"PrimitiveVariantSpec", PrimitiveVariantSpec_fbs_fields, 3};

    inline const fb::FieldDesc PrimitiveAssetSpec_fbs_fields[] = {
        {1, "name", fb::FbType::String, 4},
        {2, "geometry", fb::FbType::String, 6},
        {3, "recipe", fb::FbType::String, 8},
        {4, "primitive_type", fb::FbType::Enum, 10, fb::FbType::None, 0, nullptr, &PrimitiveType_FbsFromName, &PrimitiveType_FbsToName},
        {5, "extra_variants", fb::FbType::VectorTable, 12, fb::FbType::None, 0, &PrimitiveVariantSpec_fbs},
    };
    inline const fb::TableDesc PrimitiveAssetSpec_fbs = {"PrimitiveAssetSpec", PrimitiveAssetSpec_fbs_fields, 5};

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

    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveVariantSpec>
    EncodePrimitiveVariantSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveVariantSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveVariantSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::PrimitiveVariantSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, PrimitiveVariantSpec_fbs).o);
    }

    inline void DecodePrimitiveVariantSpec(const ::hgl::asset::schema::PrimitiveVariantSpec *root, hgl::graph::PrimitiveVariantSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            PrimitiveVariantSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveVariantSpec>::Do(ar, out);
    }

    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveAssetSpec>
    EncodePrimitiveAssetSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveAssetSpec &in)
    {
        hgl::serialize::Value node;
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForWrite(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveAssetSpec>::Do(ar, in);
        return flatbuffers::Offset<::hgl::asset::schema::PrimitiveAssetSpec>(
            hgl::serialize::fb::EncodeTable(fbb, node, PrimitiveAssetSpec_fbs).o);
    }

    inline void DecodePrimitiveAssetSpec(const ::hgl::asset::schema::PrimitiveAssetSpec *root, hgl::graph::PrimitiveAssetSpec &out)
    {
        if (!root) return;
        hgl::serialize::Value node;
        hgl::serialize::fb::DecodeTable(
            reinterpret_cast<const flatbuffers::Table *>(root),
            PrimitiveAssetSpec_fbs, node);
        hgl::serialize::Archive ar = hgl::serialize::Archive::ForRead(node);
        hgl::serialize::StructSerializer<hgl::graph::PrimitiveAssetSpec>::Do(ar, out);
    }

}//namespace hgl::asset::generated
