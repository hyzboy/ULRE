#pragma once

#include <hgl/common/PrimitiveType.h>
#include <hgl/asset/AssetAnnotation.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    /// 一个图元变体（`[[variants]]` 表）的声明。
    struct HGL_ASSET_STRUCT PrimitiveVariantSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Variant", AssetRef = "recipe", Key = "recipe")
        AnsiString              recipe_name;                ///<材质配方名（注册器按名解析）

        HGL_PROPERTY(EditAnywhere, Category = "Variant")
        PrimitiveVariantPurpose purpose = PrimitiveVariantPurpose::Surface;

        /// TOML 里的键历史上是 `lod`，字段名是 `lod_index` —— 用 `Key` 覆盖键名，
        /// 既不改 TOML 契约，也不改 C++ 命名。
        HGL_PROPERTY(EditAnywhere, Category = "Variant", Key = "lod", Range(0, 15))
        uint16_t                lod_index = 0;
    };

    /**
     * 图元资产规约（`*.primitive.toml` 解析结果，纯数据、无任何运行时句柄）。
     *
     * 图元资产是「几何 + 材质配方」的**绑定层**，刻意不自己做几何/材质解析：
     * 几何交给 `GeometryAssetRegistry`，配方交给 `MaterialRecipeRegistry`，
     * 于是同一份几何/配方可以被任意多个图元资产共享。
     *
     * ```toml
     * schema = 1
     * name = "GoldSphere"
     * geometry = "res/geo/sphere_64.geo.toml"   # `.geo.toml` / `.geo` 均可
     * recipe   = "Core.PBR.Lit"                  # 主变体配方(Surface)
     * primitive_type = "triangles"
     *
     * [[variants]]                               # 可选：额外用途变体
     * recipe  = "Core.PBR.DepthOnly"
     * purpose = "depth_only"
     * ```
     */
    struct HGL_ASSET_STRUCT PrimitiveAssetSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Identity")
        AnsiString  name;

        HGL_PROPERTY(EditAnywhere, Category = "Binding", AssetRef = "geo", Key = "geometry")
        OSString    geometry_path;                              ///<几何资产路径

        HGL_PROPERTY(EditAnywhere, Category = "Binding", AssetRef = "recipe", Key = "recipe")
        AnsiString  recipe_name;                                ///<主(Surface)变体配方名

        HGL_PROPERTY(EditAnywhere, Category = "Binding")
        PrimitiveType primitive_type = PrimitiveType::Triangles;

        HGL_PROPERTY(EditAnywhere, Category = "Binding")
        hgl::ManagedArray<PrimitiveVariantSpec> extra_variants;  ///<除主变体外的其它用途(拥有)

        bool IsValid() const { return !geometry_path.IsEmpty() && !recipe_name.IsEmpty(); }
    };

    const char *GetPrimitiveVariantPurposeName(PrimitiveVariantPurpose purpose);
    bool        ParsePrimitiveVariantPurpose(const char *name, PrimitiveVariantPurpose &out_purpose);
}//namespace hgl::graph
