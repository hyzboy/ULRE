// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/AssetGen/asset_gen.py 生成，请勿手工修改。
//   属性元数据表：供检查器 GUI、文档导出与通用批量工具消费。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/PrimitiveAssetSpec.h>

namespace hgl::asset::generated
{
    inline const PropertyMetaInfo Meta_PrimitiveVariantSpec[] =
    {
        { "recipe_name", "recipe", "recipe_name", "Variant", "材质配方名（注册器按名解析）", "AnsiString", nullptr, "recipe", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "purpose", "purpose", "purpose", "Variant", "", "PrimitiveVariantPurpose", "hgl::graph::PrimitiveVariantPurpose", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 2u, 0u, 0.0, 0.0, 0.0 },
        { "lod_index", "lod", "lod_index", "Variant", "", "uint16_t", nullptr, nullptr, PropertyKind::UInt16, PF_EditAnywhere | PF_HasRange | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 42u, 6u, 0u, 0.0, 15.0, 0.0 },
    };

    inline const ClassMetaInfo Class_PrimitiveVariantSpec =
    {
        "PrimitiveVariantSpec", "", "一个图元变体（`[[variants]]` 表）的声明。",
        Meta_PrimitiveVariantSpec, (uint32_t)(sizeof(Meta_PrimitiveVariantSpec) / sizeof(Meta_PrimitiveVariantSpec[0])),
        48u, 8u
    };

    inline const PropertyMetaInfo Meta_PrimitiveAssetSpec[] =
    {
        { "name", "name", "name", "Identity", "", "AnsiString", nullptr, nullptr, PropertyKind::String, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 0u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "geometry_path", "geometry", "geometry_path", "Binding", "几何资产路径", "OSString", nullptr, "geo", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 40u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "recipe_name", "recipe", "recipe_name", "Binding", "主(Surface)变体配方名", "AnsiString", nullptr, "recipe", PropertyKind::String, PF_EditAnywhere | PF_HasAssetRef | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 80u, 40u, 0u, 0.0, 0.0, 0.0 },
        { "primitive_type", "primitive_type", "primitive_type", "Binding", "", "PrimitiveType", "hgl::graph::PrimitiveType", nullptr, PropertyKind::Enum, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 120u, 8u, 0u, 0.0, 0.0, 0.0 },
        { "extra_variants", "extra_variants", "extra_variants", "Binding", "除主变体外的其它用途(拥有)", "hgl::ManagedArray<PrimitiveVariantSpec>", nullptr, nullptr, PropertyKind::PtrArray, PF_EditAnywhere | PF_Serialized | PF_SerializedToml | PF_SerializedFlatBuffers, 128u, 32u, 0u, 0.0, 0.0, 0.0 },
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
