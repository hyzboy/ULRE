// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   语义：**完整快照**。与 TOML 的「缺键即保留 C++ 缺省值」不同，这里的 Decode
//         会把 schema 里存在的每个字段都写回，缺省值来自类内初始化（Transient
//         字段不进 schema，因此保持类内缺省）。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <flatbuffers/flatbuffers.h>
#include <hgl/utf.h>
#include <hgl/asset/generated/primitive_schema_generated.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>

namespace hgl::asset::generated
{
    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveVariantSpec>
    EncodePrimitiveVariantSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveVariantSpec &in);
    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveAssetSpec>
    EncodePrimitiveAssetSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveAssetSpec &in);
    inline void DecodePrimitiveVariantSpec(const ::hgl::asset::schema::PrimitiveVariantSpec *t, hgl::graph::PrimitiveVariantSpec &out);
    inline void DecodePrimitiveAssetSpec(const ::hgl::asset::schema::PrimitiveAssetSpec *t, hgl::graph::PrimitiveAssetSpec &out);

    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveVariantSpec>
    EncodePrimitiveVariantSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveVariantSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _PrimitiveVariantSpec_recipe_str_offset = fbb.CreateString(in.recipe_name.c_str() ? in.recipe_name.c_str() : "");
        ::hgl::asset::schema::PrimitiveVariantSpecBuilder b(fbb);
        b.add_recipe(_PrimitiveVariantSpec_recipe_str_offset);
        b.add_purpose(static_cast<int32_t>(in.purpose));
        b.add_lod(in.lod_index);
        return b.Finish();
    }

    inline flatbuffers::Offset<::hgl::asset::schema::PrimitiveAssetSpec>
    EncodePrimitiveAssetSpec(flatbuffers::FlatBufferBuilder &fbb, const hgl::graph::PrimitiveAssetSpec &in)
    {
        // FlatBuffers 要求父表开始前子表/字符串/向量都已建好。
        const auto _PrimitiveAssetSpec_name_str_offset = fbb.CreateString(in.name.c_str() ? in.name.c_str() : "");
        const hgl::U8String _PrimitiveAssetSpec_geometry_str_u8 = hgl::ToU8String(in.geometry_path);
        const auto _PrimitiveAssetSpec_geometry_str_offset = fbb.CreateString(reinterpret_cast<const char *>(_PrimitiveAssetSpec_geometry_str_u8.c_str() ? _PrimitiveAssetSpec_geometry_str_u8.c_str() : U8_TEXT("")));
        const auto _PrimitiveAssetSpec_recipe_str_offset = fbb.CreateString(in.recipe_name.c_str() ? in.recipe_name.c_str() : "");
        const auto _PrimitiveAssetSpec_extra_variants_vec = fbb.CreateVector<flatbuffers::Offset<::hgl::asset::schema::PrimitiveVariantSpec>>((size_t)in.extra_variants.GetCount(), [&](size_t i, const hgl::graph::PrimitiveAssetSpec *source) { return EncodePrimitiveVariantSpec(fbb, *source->extra_variants[(int)i]); }, &in);
        ::hgl::asset::schema::PrimitiveAssetSpecBuilder b(fbb);
        b.add_name(_PrimitiveAssetSpec_name_str_offset);
        b.add_geometry(_PrimitiveAssetSpec_geometry_str_offset);
        b.add_recipe(_PrimitiveAssetSpec_recipe_str_offset);
        b.add_primitive_type(static_cast<int32_t>(in.primitive_type));
        b.add_extra_variants(_PrimitiveAssetSpec_extra_variants_vec);
        return b.Finish();
    }

    inline void DecodePrimitiveVariantSpec(const ::hgl::asset::schema::PrimitiveVariantSpec *t, hgl::graph::PrimitiveVariantSpec &out)
    {
        if (!t) return;
        if (const ::flatbuffers::String *s = t->recipe()) out.recipe_name = s->str().c_str();
        out.purpose = static_cast<hgl::graph::PrimitiveVariantPurpose>(t->purpose());
        out.lod_index = t->lod();
    }

    inline void DecodePrimitiveAssetSpec(const ::hgl::asset::schema::PrimitiveAssetSpec *t, hgl::graph::PrimitiveAssetSpec &out)
    {
        if (!t) return;
        if (const ::flatbuffers::String *s = t->name()) out.name = s->str().c_str();
        if (const ::flatbuffers::String *s = t->geometry()) out.geometry_path = hgl::ToOSString(s->str().c_str());
        if (const ::flatbuffers::String *s = t->recipe()) out.recipe_name = s->str().c_str();
        out.primitive_type = static_cast<hgl::graph::PrimitiveType>(t->primitive_type());
        if (const auto *v = t->extra_variants())
        {
            out.extra_variants.Clear();
            for (uint32_t i = 0; i < v->size(); ++i)
            {
                auto *item = out.extra_variants.Create();
                DecodePrimitiveVariantSpec(v->Get(i), *item);
            }
        }
    }

} // namespace hgl::asset::generated
