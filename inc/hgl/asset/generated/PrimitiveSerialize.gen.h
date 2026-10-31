// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   这是**唯一一份**字段遍历代码：它只说「哪个字段、什么名字、什么 id」，
//   不关心落到 TOML 还是 FlatBuffers。新增格式后端不需要重新生成任何东西。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/serialize/Archive.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>
#include <hgl/asset/generated/PrimitiveMeta.gen.h>

namespace hgl::serialize
{
    template<> struct EnumTraits<hgl::graph::PrimitiveVariantPurpose>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::PrimitiveVariantPurpose v)
        { return hgl::asset::generated::AssetEnumName_PrimitiveVariantPurpose(v); }
        static bool FromName(const char *s, hgl::graph::PrimitiveVariantPurpose &out)
        { return hgl::asset::generated::AssetEnumParse_PrimitiveVariantPurpose(s, out); }
    };

    template<> struct EnumTraits<hgl::graph::PrimitiveType>
    {
        static constexpr bool defined = true;
        static const char *ToName(hgl::graph::PrimitiveType v)
        { return hgl::asset::generated::AssetEnumName_PrimitiveType(v); }
        static bool FromName(const char *s, hgl::graph::PrimitiveType &out)
        { return hgl::asset::generated::AssetEnumParse_PrimitiveType(s, out); }
    };

    template<> struct StructSerializer<hgl::graph::PrimitiveVariantSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "recipe", v.recipe_name);
            ar.Field(2, "purpose", v.purpose);
            ar.Field(3, "lod", v.lod_index);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_PrimitiveVariantSpec; }
    };

    template<> struct StructSerializer<hgl::graph::PrimitiveAssetSpec>
    {
        static constexpr bool defined = true;

        template<typename Self>
        static void Do(Archive &ar, Self &v)
        {
            ar.Field(1, "name", v.name);
            ar.Field(2, "geometry", v.geometry_path);
            ar.Field(3, "recipe", v.recipe_name);
            ar.Field(4, "primitive_type", v.primitive_type);
            ar.Field(5, "extra_variants", v.extra_variants);
        }
        static const hgl::asset::ClassMetaInfo *Meta()
        { return &hgl::asset::generated::Class_PrimitiveAssetSpec; }
    };

}//namespace hgl::serialize
