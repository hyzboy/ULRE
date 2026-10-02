#pragma once

#include<hgl/ecs/core/ComponentRow.h>
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
     * 即原图元渲染组件的几何/资产侧状态。资源引用（`PrimitiveAsset`）**非拥有**，
     * asset 生命周期由作者/资源管理器决定。
     *
     * 渲染侧依赖（管线缓存 / render_item 4-ID / 多实例绑定）**不在本组件**——A5b 删除渲染组件后
     * 归世界 `MaterialRuntimeTable` 的每实例 slot（见 support/PrimitiveState.h）。变体/asset 变化
     * 会经 `InvalidateOwnerRuntimePipeline()` 作废同实体 slot 上按 RenderPass 解析出的管线缓存。
     *
     * A5b：渲染组件删除后，本组件是"该实体是一个 Primitive 渲染元素"的**唯一标记** ⇒
     * **类型表**声明系统组 `Primitive`（渲染图据此为该世界启用图元 pass；与 LinesComponent 的 `Line` 同构）。
     * T2-c：组归属是**类型属性**（`kComponentTypeTable[].group`），不再由组件对象上的虚函数声明。
     *
     * 世界访问器：`ECSContext::GetGeometryData(EntityID)` / `GetOrCreateGeometryData(EntityID)`。
     */
    class GeometryData : public ComponentRow
    {
    private:

        const hgl::graph::PrimitiveAsset *primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        uint32_t primitiveVariantIndex = 0;
        hgl::graph::PrimitiveVariantPurpose primitiveVariantPurpose =
            hgl::graph::PrimitiveVariantPurpose::Surface;

        /// 几何/变体变化 ⇒ 同实体 slot 上按-pass 解析出的运行期管线不再可信（A5b：管线缓存归 slot）
        void InvalidateOwnerRuntimePipeline();

    public:

        GeometryData() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~GeometryData() = default;

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

        // ── 几何行号（GPU 可用形态；B3-iii 取代原先的三个运行期指针）──
        /// 几何在设备级 `MeshDrawParams` 池里的行号（= 4-ID 的 `geometry_id`）；0 = 无几何。
        /// **按需从 asset 的几何反算**（几何是行的宿主，行号由它在创建期取得）——
        /// 本组件不缓存第二份，也就不会出现"几何换了行号、组件里还是旧的"这种漂移。
        uint32_t GetGeometryID() const;

        // Asset 里的默认配方——材质解析链的**基底来源**。作者把材质授权搬到数据层
        // （MaterialData）之后，本访问器仍是 asset 归属的数据，由调用方作为
        // `MaterialData::BuildResolvedRecipe` 的 asset_default_recipe 传入。
        const hgl::graph::mtl::MaterialRecipe *GetAssetMaterialRecipe() const;

        // 本地包围盒 / 包围球半径（几何派生量）
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        /// 包围球半径（本地 AABB 的**半对角线长**；不含实体缩放；渲染侧视锥剔除用）。
        /// B3-ii：**不缓存第二份** —— 与 `GeometryAABB` 行同源同式（都取自 asset 几何的本地 AABB），
        /// 按需反算即可，消掉「两处各算一份、改一处会漂」的风险。
        float GetBoundingRadius() const;

    public:

        void OnDetach();          ///< 卸载钩子（非虚：由类型化池在析构前调用）
    };

    /// 槽位映射：`GeometryData` 承载几何来源（v2 §3 的 GPU 可见组件之一；scope Global）
    template<> struct ComponentTypeOf<GeometryData> { static constexpr ComponentType value = ComponentType::Geometry; };
}//namespace hgl::ecs
