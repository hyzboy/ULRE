// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出、严格键校验与通用格式后端消费。
//   它是「结构长什么样」的**唯一运行期真源**，与格式无关。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>
#include <hgl/common/PrimitiveType.h>

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
    { return hgl::graph::GetPrimitiveTypeName(v); }

    inline bool AssetEnumParse_PrimitiveType(const char *s, hgl::graph::PrimitiveType &out)
    { return hgl::graph::ParsePrimitiveTypeName(s, out); }

    inline const char *AssetEnumName64_PrimitiveVariantPurpose(const int64_t v)
    { return AssetEnumName_PrimitiveVariantPurpose(static_cast<hgl::graph::PrimitiveVariantPurpose>(v)); }

    inline bool AssetEnumParse64_PrimitiveVariantPurpose(const char *s, int64_t &out)
    {
        hgl::graph::PrimitiveVariantPurpose tmp{};
        if (!AssetEnumParse_PrimitiveVariantPurpose(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    inline const char *AssetEnumName64_PrimitiveType(const int64_t v)
    { return AssetEnumName_PrimitiveType(static_cast<hgl::graph::PrimitiveType>(v)); }

    inline bool AssetEnumParse64_PrimitiveType(const char *s, int64_t &out)
    {
        hgl::graph::PrimitiveType tmp{};
        if (!AssetEnumParse_PrimitiveType(s, tmp)) return false;
        out = static_cast<int64_t>(tmp);
        return true;
    }

    extern const ClassMetaInfo Class_PrimitiveVariantSpec;
    extern const ClassMetaInfo Class_PrimitiveAssetSpec;

    inline const PropertyMetaInfo Meta_PrimitiveVariantSpec[] =
    {
        { "recipe_name", "recipe", "recipe_name", "Variant", "材质配方名（注册器按名解析）", "AnsiString", nullptr, "recipe", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "purpose", "purpose", "purpose", "Variant", "", "PrimitiveVariantPurpose", "hgl::graph::PrimitiveVariantPurpose", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 2u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_PrimitiveVariantPurpose, &AssetEnumName64_PrimitiveVariantPurpose },
        { "lod_index", "lod", "lod_index", "Variant", "", "uint16_t", nullptr, nullptr, PropertyKind::UInt16, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 42u, 6u, 0u, 0.0, 15.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_PrimitiveVariantSpec =
    {
        "PrimitiveVariantSpec", "", "一个图元变体（`[[variants]]` 表）的声明。",
        Meta_PrimitiveVariantSpec, (uint32_t)(sizeof(Meta_PrimitiveVariantSpec) / sizeof(Meta_PrimitiveVariantSpec[0])),
        48u, 8u
    };

    inline const PropertyMetaInfo Meta_PrimitiveAssetSpec[] =
    {
        { "schema", "schema", "schema", "Identity", "", "uint32_t", nullptr, nullptr, PropertyKind::UInt32, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 8u, 0u, 1.0, 1.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "name", "name", "name", "Identity", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 8u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "geometry_path", "geometry", "geometry_path", "Binding", "几何资产路径", "OSString", nullptr, "geo", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 48u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "recipe_name", "recipe", "recipe_name", "Binding", "主(Surface)变体配方名", "AnsiString", nullptr, "recipe", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 88u, 40u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, nullptr, nullptr },
        { "primitive_type", "primitive_type", "primitive_type", "Binding", "", "PrimitiveType", "hgl::graph::PrimitiveType", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 128u, 8u, 0u, 0.0, 0.0, 0.0, nullptr, PropertyKind::None, &AssetEnumParse64_PrimitiveType, &AssetEnumName64_PrimitiveType },
        { "extra_variants", "variants", "extra_variants", "Binding", "除主变体外的其它用途(拥有)", "hgl::ManagedArray<PrimitiveVariantSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 136u, 24u, 0u, 0.0, 0.0, 0.0, &Class_PrimitiveVariantSpec, PropertyKind::Struct, nullptr, nullptr },
    };

    inline const ClassMetaInfo Class_PrimitiveAssetSpec =
    {
        "PrimitiveAssetSpec", "", "图元资产规约（`*.primitive.toml` 解析结果，纯数据、无任何运行时句柄）。 图元资产是「几何 + 材质配方」的**绑定层**，刻意不自己做几何/材质解析： 几何交给 `GeometryAssetRegistry`，配方交给 `MaterialRecipeRegistry`， 于是同一份几何/配方可以被任意多个图元资产共享。 ```toml schema = 1 name = \"GoldSphere\" geometry = \"res/geo/sphere_64.geo.toml\"   # `.geo.toml` / `.geo` 均可 recipe   = \"Core.PBR.Lit\"                  # 主变体配方(Surface) primitive_type = \"triangles\" [[variants]]                               # 可选：额外用途变体 recipe  = \"Core.PBR.DepthOnly\" purpose = \"depth_only\" ```",
        Meta_PrimitiveAssetSpec, (uint32_t)(sizeof(Meta_PrimitiveAssetSpec) / sizeof(Meta_PrimitiveAssetSpec[0])),
        160u, 8u
    };

    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::PrimitiveVariantSpec *) { return &Class_PrimitiveVariantSpec; }
    inline const ClassMetaInfo *GetClassMeta(const hgl::graph::PrimitiveAssetSpec *) { return &Class_PrimitiveAssetSpec; }
} // namespace hgl::asset::generated
