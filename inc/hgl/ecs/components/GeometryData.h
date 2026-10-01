#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/support/ComponentTypeTable.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<cstdint>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace math
    {
        class AABB;
    }

    namespace graph
    {
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class Geometry;
        class ShaderProgram;
    }
}

namespace hgl::ecs
{
    /**
     * GeometryData —— 几何**资产侧**数据层（v2 §9.1-3 / A5a）。
     *
     * 职责：承载"这块可渲染物用什么几何、选哪个变体、运行期解析出的几何绑定"，
     * 即原 `PrimitiveComponent` 的几何/资产侧状态。资源引用（`PrimitiveAsset`）**非拥有**，
     * asset 生命周期由作者/资源管理器决定。
     *
     * 渲染侧依赖（管线缓存 / render_item 4-ID）**不在本组件**——仍留在 `PrimitiveComponent`
     * （A5b 处理）。变体/asset 变化会经 `InvalidateOwnerRuntimePipeline()` 让同实体的
     * `PrimitiveComponent` 作废它按 RenderPass 解析出的管线缓存。
     *
     * 世界访问器：`ECSContext::GetGeometryData(EntityID)` / `GetOrCreateGeometryData(EntityID)`。
     */
    class GeometryData : public Component
    {
    private:

        const hgl::graph::PrimitiveAsset *primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        uint32_t primitiveVariantIndex = 0;
        hgl::graph::PrimitiveVariantPurpose primitiveVariantPurpose =
            hgl::graph::PrimitiveVariantPurpose::Surface;
        hgl::graph::GeometryDataBuffer *runtime_data_buffer = nullptr;
        hgl::graph::GeometryDrawRange *runtime_draw_range = nullptr;
        hgl::graph::Geometry *runtime_geometry = nullptr;

        /// 由本地 AABB 求得的包围球半径（不含实体缩放；渲染侧视锥剔除用）。
        /// 原挂在 `RenderableComponent::boundingRadius`，唯一写者就是 `SetPrimitiveAsset` ⇒ 随几何状态迁来。
        float bounding_radius = 0.0f;

        /// 几何/变体变化 ⇒ 同实体 `PrimitiveComponent` 的按-pass 管线缓存不再可信
        void InvalidateOwnerRuntimePipeline();

    public:

        explicit GeometryData(const std::string& name = "GeometryData")
            : Component(name)
        {
        }

        ~GeometryData() override = default;

    public:

        // ── 资产（几何 + 默认配方）──
        void SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset);
        const hgl::graph::PrimitiveAsset *GetPrimitiveAsset() const { return primitiveAsset; }
        void ClearPrimitiveAsset() { SetPrimitiveAsset(nullptr); }

        // ── 变体选择 ──
        void SetPrimitiveVariantIndex(const uint32_t index) { primitiveVariantIndex = index; }
        uint32_t GetPrimitiveVariantIndex() const { return primitiveVariantIndex; }
        void SetPrimitiveVariantPurpose(const hgl::graph::PrimitiveVariantPurpose purpose);
        hgl::graph::PrimitiveVariantPurpose GetPrimitiveVariantPurpose() const { return primitiveVariantPurpose; }

        // ── 运行期几何绑定（渲染侧解析产物）──
        bool EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material);
        void ClearRuntimeGeometryBinding();
        const hgl::graph::GeometryDataBuffer *GetRuntimeGeometryDataBuffer() const { return runtime_data_buffer; }
        const hgl::graph::GeometryDrawRange *GetRuntimeGeometryDrawRange() const { return runtime_draw_range; }

        // Asset 里的默认配方——材质解析链的**基底来源**。作者把材质授权搬到数据层
        // （MaterialData）之后，本访问器仍是 asset 归属的数据，由调用方作为
        // `MaterialData::BuildResolvedRecipe` 的 asset_default_recipe 传入。
        const hgl::graph::mtl::MaterialRecipe *GetAssetMaterialRecipe() const;

        // 本地包围盒 / 包围球半径（几何派生量）
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;
        float GetBoundingRadius() const { return bounding_radius; }

    public:

        void OnDetach() override;
    };

    /// 槽位映射：`GeometryData` 承载几何来源（v2 §3 的 GPU 可见组件之一；scope Global）
    template<> struct ComponentTypeOf<GeometryData> { static constexpr ComponentType value = ComponentType::Geometry; };
}//namespace hgl::ecs
