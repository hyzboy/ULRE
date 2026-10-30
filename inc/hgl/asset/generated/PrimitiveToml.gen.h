// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   语义：键缺失 = 保留 C++ 类内初始值；键存在但类型/长度不符 = 解析失败。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <toml/toml.hpp>
#include <hgl/asset/AssetSerialize.h>
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

    inline bool ReadPrimitiveVariantSpec(const toml::value &table, hgl::graph::PrimitiveVariantSpec &out);
    inline bool ReadPrimitiveAssetSpec(const toml::value &table, hgl::graph::PrimitiveAssetSpec &out);
    inline toml::value WritePrimitiveVariantSpec(const hgl::graph::PrimitiveVariantSpec &in);
    inline toml::value WritePrimitiveAssetSpec(const hgl::graph::PrimitiveAssetSpec &in);

    inline bool ReadPrimitiveVariantSpec(const toml::value &table, hgl::graph::PrimitiveVariantSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "recipe"))
        {
            if (!detail::AsString(*v, out.recipe_name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "purpose"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_PrimitiveVariantPurpose((*v).as_string().c_str(), out.purpose)) return false;
            }
            else if ((*v).is_integer())
            {
                out.purpose = static_cast<hgl::graph::PrimitiveVariantPurpose>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "lod"))
        {
            if (!detail::AsUnsigned(*v, out.lod_index)) return false;
        }
        return true;
    }

    inline bool ReadPrimitiveAssetSpec(const toml::value &table, hgl::graph::PrimitiveAssetSpec &out)
    {
        if (!table.is_table()) return false;
        if (const toml::value *v = detail::Find(table, "name"))
        {
            if (!detail::AsString(*v, out.name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "geometry"))
        {
            if (!detail::AsString(*v, out.geometry_path)) return false;
        }
        if (const toml::value *v = detail::Find(table, "recipe"))
        {
            if (!detail::AsString(*v, out.recipe_name)) return false;
        }
        if (const toml::value *v = detail::Find(table, "primitive_type"))
        {
            if ((*v).is_string())
            {
                if (!AssetEnumParse_PrimitiveType((*v).as_string().c_str(), out.primitive_type)) return false;
            }
            else if ((*v).is_integer())
            {
                out.primitive_type = static_cast<hgl::graph::PrimitiveType>((*v).as_integer());
            }
            else
                return false;
        }
        if (const toml::value *v = detail::Find(table, "extra_variants"))
        {
            if (!(*v).is_array()) return false;
            {
                const auto &arr = (*v).as_array();
                out.extra_variants.Clear();
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    auto *item = out.extra_variants.Create();
                    if (!ReadPrimitiveVariantSpec(arr[i], *item)) return false;
                }
            }
        }
        return true;
    }

    inline toml::value WritePrimitiveVariantSpec(const hgl::graph::PrimitiveVariantSpec &in)
    {
        toml::table t;
        t["recipe"] = detail::FromString(in.recipe_name);
        {
            const char *nm = AssetEnumName_PrimitiveVariantPurpose(in.purpose);
            t["purpose"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.purpose);
        }
        t["lod"] = detail::FromInteger(in.lod_index);
        return toml::value(std::move(t));
    }

    inline toml::value WritePrimitiveAssetSpec(const hgl::graph::PrimitiveAssetSpec &in)
    {
        toml::table t;
        t["name"] = detail::FromString(in.name);
        t["geometry"] = detail::FromString(in.geometry_path);
        t["recipe"] = detail::FromString(in.recipe_name);
        {
            const char *nm = AssetEnumName_PrimitiveType(in.primitive_type);
            t["primitive_type"] = nm
                ? toml::value(toml::value::string_type(nm))
                : detail::FromInteger((int64_t)in.primitive_type);
        }
        {
            toml::array a;
            a.reserve((size_t)in.extra_variants.GetCount());
            for (int i = 0; i < in.extra_variants.GetCount(); ++i)
                a.push_back(toml::value(WritePrimitiveVariantSpec(*in.extra_variants[i])));
            t["extra_variants"] = toml::value(std::move(a));
        }
        return toml::value(std::move(t));
    }

} // namespace hgl::asset::generated
