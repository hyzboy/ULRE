#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/mtl/MaterialRecipe.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * Sprite2DSyncSystem -- Sprite2DComponent 到场景管线的桥（Route A）
     *
     * TickPostCamera 相位（依赖 Transform2DSystem：须在 2D 层级求值之后读世界值，
     * 并在 RenderCollect 读取世界矩阵之前写 3D TRS）。对每个挂 Sprite2DComponent
     * 的实体每帧：
     *   1. 保证载体齐备：Transform2D（缺则补挂）、3D Transform（Movable）、
     *      GeometryData（共享单位矩形 PrimitiveAsset）、MaterialData；
     *   2. 把 world_bounds（屏幕像素，原点左下 +Y 上）换算成 NDC 的
     *      TRS（平移 + 各向异性缩放）写入局部变换；
     *   3. texture_generation 变化时把覆盖纹理下发到 MaterialData 的 base_color 槽。
     *
     * 限制（桥接形态固有）：
     *   · 精灵本体不随 2D 旋转而旋转（NDC 各向异性，TRS 表达不了"旋转×非均匀缩放"），
     *     父子旋转只影响世界位置与 AABB；
     *   · 材质侧没有纹理移除入口：清除覆盖后，已下发的覆盖纹理保留。
     */
    class Sprite2DSyncSystem : public System
    {
    private:
        ECSContext *world = nullptr;

        bool resources_ready = false;      ///< 共享资产已建立
        bool warned_no_resources = false;  ///< 建不起来只记一次日志

        hgl::graph::mtl::MaterialRecipe recipe_copy{};  ///< 配方快照（PrimitiveAsset 需稳定指针）
        hgl::graph::PrimitiveAsset      rect_asset{};   ///< 单位矩形图元资产（全部精灵共享）

    public:
        explicit Sprite2DSyncSystem(const AnsiString& name = "Sprite2DSyncSystem");
        ~Sprite2DSyncSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;

        /// 共享单位矩形图元资产（演示/调试可直接引用）
        const hgl::graph::PrimitiveAsset *GetRectAsset() const
        {
            return resources_ready ? &rect_asset : nullptr;
        }

    private:
        bool EnsureResources();
    };
}//namespace hgl::ecs