#pragma once

#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/asset/GeometryAsset.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/PrimitiveType.h>

namespace hgl::graph
{
    class GeometryAsset;

    enum class PrimitiveVariantPurpose : uint8_t
    {
        Surface = 0,
        DepthOnly,
        ShadowCaster,
        Picking,
        Debug
    };

    struct PrimitiveVariant
    {
        const mtl::MaterialRecipe *material_recipe = nullptr;
        PrimitiveVariantPurpose purpose = PrimitiveVariantPurpose::Surface;
        uint16_t lod_index = 0;

        bool IsValid() const { return material_recipe != nullptr; }

        /// 容器（`ValueArray`）内部的查找需要相等比较：按配方指针 + 用途 + LOD 判定。
        bool operator==(const PrimitiveVariant &rhs) const
        {
            return material_recipe == rhs.material_recipe
                && purpose == rhs.purpose
                && lod_index == rhs.lod_index;
        }

        bool operator!=(const PrimitiveVariant &rhs) const { return !(*this == rhs); }
    };

    /**
     * PrimitiveAsset — immutable asset-level pairing of Geometry + PrimitiveVariant[].
     *
     * Design intent:
     * - asset-only data
     * - serializable by reference (Geometry asset ref + PrimitiveVariant asset refs)
     * - shared by multiple entities / runtime draw objects
     *
     * It intentionally does NOT carry runtime-only state such as:
     * - ShaderProgram / MaterialInstance
     * - DescriptorBindingSet
     * - Pipeline
     * - SSBO rows / bindless handles
     */
    class PrimitiveAsset
    {
    private:
        Geometry *geometry_ = nullptr;
        /// 几何的**资产句柄**（非拥有，可为空）。为空表示「无从判断就绪度」，
        /// 门禁按「已就绪」放行，保持手工 `PrimitiveAsset(Geometry*,...)` 的既有行为。
        const GeometryAsset *geometry_asset_ = nullptr;
        PrimitiveType primitive_type_ = PrimitiveType::Triangles;
        PrimitiveVariant inline_variant_{};
        const PrimitiveVariant *variants_ = nullptr;
        uint32_t variant_count_ = 0;

    private:
        void CopyFrom(const PrimitiveAsset &src)
        {
            geometry_ = src.geometry_;
            geometry_asset_ = src.geometry_asset_;
            primitive_type_ = src.primitive_type_;
            inline_variant_ = src.inline_variant_;
            variant_count_ = src.variant_count_;

            if (!src.variants_ || src.variant_count_ == 0)
            {
                variants_ = nullptr;
                return;
            }

            variants_ = (src.variants_ == &src.inline_variant_) ? &inline_variant_ : src.variants_;
        }

        void MoveFrom(PrimitiveAsset &src)
        {
            CopyFrom(src);

            src.geometry_ = nullptr;
            src.geometry_asset_ = nullptr;
            src.primitive_type_ = PrimitiveType::Triangles;
            src.inline_variant_ = {};
            src.variants_ = nullptr;
            src.variant_count_ = 0;
        }

    public:
        PrimitiveAsset() = default;
        PrimitiveAsset(const PrimitiveAsset &src) { CopyFrom(src); }
        PrimitiveAsset(PrimitiveAsset &&src) noexcept { MoveFrom(src); }

        PrimitiveAsset(Geometry *geo,
                       const PrimitiveVariant *variants,
                       const uint32_t variant_count,
                       const PrimitiveType primitive_type = PrimitiveType::Triangles)
            : geometry_(geo), primitive_type_(primitive_type), variants_(variants), variant_count_(variant_count) {}

        PrimitiveAsset(Geometry *geo,
                       const mtl::MaterialRecipe *recipe,
                       const PrimitiveType primitive_type = PrimitiveType::Triangles)
            : geometry_(geo), primitive_type_(primitive_type)
        {
            inline_variant_.material_recipe = recipe;
            variants_ = recipe ? &inline_variant_ : nullptr;
            variant_count_ = recipe ? 1u : 0u;
        }

        /// 带几何资产句柄的构造：门禁可据此判断几何是否就绪。
        PrimitiveAsset(Geometry *geo,
                       const GeometryAsset *geometry_asset,
                       const PrimitiveVariant *variants,
                       const uint32_t variant_count,
                       const PrimitiveType primitive_type = PrimitiveType::Triangles)
            : geometry_(geo), geometry_asset_(geometry_asset),
              primitive_type_(primitive_type), variants_(variants), variant_count_(variant_count) {}

        PrimitiveAsset &operator=(const PrimitiveAsset &src)
        {
           if (this != &src)
               CopyFrom(src);

           return *this;
        }

        PrimitiveAsset &operator=(PrimitiveAsset &&src) noexcept
        {
           if (this != &src)
               MoveFrom(src);

           return *this;
        }

        bool IsValid() const { return geometry_ != nullptr && variant_count_ > 0 && variants_ != nullptr && variants_[0].IsValid(); }

        /**
         * 是否**已声明**（配方与变体齐全），但几何可能还没就绪。
         *
         * 异步几何的存在使「已声明」与「可画」必须分开：
         * - 图元资产注册器装配时要 `IsDeclared()`（几何还在后台读盘是正常状态）；
         * - 渲染门禁才用 `IsGeometryReady()` / `IsValid()`。
         */
        bool IsDeclared() const
        {
            return (geometry_ != nullptr || geometry_asset_ != nullptr)
                && variant_count_ > 0 && variants_ != nullptr && variants_[0].IsValid();
        }

        Geometry *GetGeometry() const { return geometry_; }
        const GeometryAsset *GetGeometryAsset() const { return geometry_asset_; }
        PrimitiveType GetPrimitiveType() const { return primitive_type_; }
        uint32_t GetVariantCount() const { return variant_count_; }

        /**
         * 几何是否**已就绪可画**。
         *
         * 没有几何资产句柄（手工 `PrimitiveAsset(Geometry*, recipe)` 构造）⇒ 视为就绪，
         * 保持既有行为；有句柄则以句柄状态为准 —— 这正是异步流送的门禁依据。
         */
        bool IsGeometryReady() const
        {
            return geometry_ && (!geometry_asset_ || geometry_asset_->IsReady());
        }

        const PrimitiveVariant *GetVariant(const uint32_t index) const
        {
            if (!variants_ || index >= variant_count_)
                return nullptr;

            return variants_ + index;
        }

        const PrimitiveVariant *GetDefaultVariant() const
        {
            return GetVariant(0);
        }

        const PrimitiveVariant *FindVariantByPurpose(
            const PrimitiveVariantPurpose purpose,
            const uint32_t preferred_index = 0) const
        {
            const PrimitiveVariant *preferred =
                GetVariant(preferred_index);
            if (preferred
             && preferred->IsValid()
             && preferred->purpose == purpose)
                return preferred;

            for (uint32_t i = 0; i < variant_count_; ++i)
            {
                const PrimitiveVariant *variant = GetVariant(i);
                if (variant
                 && variant->IsValid()
                 && variant->purpose == purpose)
                    return variant;
            }

            if (purpose != PrimitiveVariantPurpose::Surface)
            {
                for (uint32_t i = 0; i < variant_count_; ++i)
                {
                    const PrimitiveVariant *variant = GetVariant(i);
                    if (variant
                     && variant->IsValid()
                     && variant->purpose
                        == PrimitiveVariantPurpose::Surface)
                        return variant;
                }
            }

            return preferred && preferred->IsValid()
                ? preferred : GetDefaultVariant();
        }

        const mtl::MaterialRecipe *GetMaterialRecipe() const
        {
            const PrimitiveVariant *variant = GetDefaultVariant();
            return variant ? variant->material_recipe : nullptr;
        }
    };
}//namespace hgl::graph
