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

namespace hgl::asset::generated
{
    inline const char *AssetEnumName_PrimitiveVariantPurpose(const hgl::graph::PrimitiveVariantPurpose v)
    {
        switch (v)
        {
        case hgl::graph::PrimitiveVariantPurpose::Surface: return "surface";
        case hgl::graph::PrimitiveVariantPurpose::DepthOnly: return "depth_only";
        case hgl::graph::PrimitiveVariantPurpose::ShadowCaster: return "shadow_caster";
        case hgl::graph::PrimitiveVariantPurpose::Picking: return "picking";
        case hgl::graph::PrimitiveVariantPurpose::Debug: return "debug";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_PrimitiveVariantPurpose(const char *s, hgl::graph::PrimitiveVariantPurpose &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "surface") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Surface; return true; }
        if (hgl::stricmp(s, "Surface") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Surface; return true; }
        if (hgl::stricmp(s, "depth_only") == 0) { out = hgl::graph::PrimitiveVariantPurpose::DepthOnly; return true; }
        if (hgl::stricmp(s, "DepthOnly") == 0) { out = hgl::graph::PrimitiveVariantPurpose::DepthOnly; return true; }
        if (hgl::stricmp(s, "shadow_caster") == 0) { out = hgl::graph::PrimitiveVariantPurpose::ShadowCaster; return true; }
        if (hgl::stricmp(s, "ShadowCaster") == 0) { out = hgl::graph::PrimitiveVariantPurpose::ShadowCaster; return true; }
        if (hgl::stricmp(s, "picking") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Picking; return true; }
        if (hgl::stricmp(s, "Picking") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Picking; return true; }
        if (hgl::stricmp(s, "debug") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Debug; return true; }
        if (hgl::stricmp(s, "Debug") == 0) { out = hgl::graph::PrimitiveVariantPurpose::Debug; return true; }
        return false;
    }

    inline const char *AssetEnumName_PrimitiveType(const hgl::graph::PrimitiveType v)
    {
        switch (v)
        {
        case hgl::graph::PrimitiveType::Points: return "Points";
        case hgl::graph::PrimitiveType::Lines: return "Lines";
        case hgl::graph::PrimitiveType::LineStrip: return "LineStrip";
        case hgl::graph::PrimitiveType::Triangles: return "Triangles";
        case hgl::graph::PrimitiveType::TriangleStrip: return "TriangleStrip";
        case hgl::graph::PrimitiveType::Fan: return "Fan";
        case hgl::graph::PrimitiveType::Error: return "Error";
        }
        return nullptr;
    }

    inline bool AssetEnumParse_PrimitiveType(const char *s, hgl::graph::PrimitiveType &out)
    {
        if (!s) return false;
        if (hgl::stricmp(s, "Points") == 0) { out = hgl::graph::PrimitiveType::Points; return true; }
        if (hgl::stricmp(s, "Lines") == 0) { out = hgl::graph::PrimitiveType::Lines; return true; }
        if (hgl::stricmp(s, "LineStrip") == 0) { out = hgl::graph::PrimitiveType::LineStrip; return true; }
        if (hgl::stricmp(s, "Triangles") == 0) { out = hgl::graph::PrimitiveType::Triangles; return true; }
        if (hgl::stricmp(s, "TriangleStrip") == 0) { out = hgl::graph::PrimitiveType::TriangleStrip; return true; }
        if (hgl::stricmp(s, "Fan") == 0) { out = hgl::graph::PrimitiveType::Fan; return true; }
        if (hgl::stricmp(s, "Error") == 0) { out = hgl::graph::PrimitiveType::Error; return true; }
        return false;
    }

}//namespace hgl::asset::generated

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
    };

}//namespace hgl::serialize
