#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<cstdint>

namespace hgl::graph
{
    class Texture;
    class TextureAsset;
}

namespace hgl::ecs
{
    /**
     * Sprite2DComponent -- 2D 精灵（图片载体）声明组件
     *
     * 用法：实体挂 Transform2DComponent（层级树节点）+ Sprite2DComponent（本组件），
     * Sprite2DSyncSystem 每帧把该节点桥接为场景管线里的 UnlitTexture 单位矩形：
     *   · 几何：共享单位矩形 [0,1]²（ZeroOneToNDC 映射进 NDC）；
     *   · 位置/尺寸：world_bounds（世界像素 AABB，原点左下角 +Y 向上）换算成
     *     NDC 的 TRS（在 TickPostCamera 写入，先于 RenderCollect 读取世界矩阵）；
     *   · 纹理：配方 Sprite2D.UnlitTexture 提供默认图；本组件的 texture /
     *     texture_asset 覆盖同名 base_color 槽（同一配方下的实例换图）。
     *
     * 限制（Route A 桥接形态）：精灵本体不随 2D 旋转而旋转（TRS 只携带
     * 平移+各向异性缩放）；父子旋转仅影响世界位置与 AABB。
     *
     * 两种纹理来源二选一（都留空 = 用配方默认图）：
     *   · texture       —— 已就绪的裸纹理（归属 TextureManager，本组件不持所有权）；
     *   · texture_asset —— 资产句柄，可为 MetaOnly（仅文件名），由流送系统自动加载。
     */
    class Sprite2DComponent : public ComponentRow
    {
    public:

        hgl::graph::Texture      *texture       = nullptr;   ///< 覆盖纹理（已就绪），null = 不覆盖
        hgl::graph::TextureAsset *texture_asset = nullptr;   ///< 覆盖纹理句柄（可 MetaOnly），null = 不覆盖

        /// 纹理授权代：每次 Set* 递增；同步系统据此跳过重复下发
        uint32_t texture_generation = 0;

        /// 最近一次已下发到材质数据的代（仅 Sprite2DSyncSystem 写）
        uint32_t synced_generation = 0;

    public:

        Sprite2DComponent() = default;

        ~Sprite2DComponent() = default;

        hgl::graph::Texture *GetTexture() const { return texture; }
        hgl::graph::TextureAsset *GetTextureAsset() const { return texture_asset; }
        bool HasTextureOverride() const { return texture != nullptr || texture_asset != nullptr; }

        /// 用已就绪纹理覆盖 base_color（传 null 清除覆盖，回退配方默认图）
        void SetTexture(hgl::graph::Texture *tex)
        {
            if (tex == texture && texture_asset == nullptr) return;
            texture = tex;
            texture_asset = nullptr;
            ++texture_generation;
        }

        /// 用资产句柄覆盖 base_color（MetaOnly 句柄自动流送；传 null 清除覆盖）
        void SetTextureAsset(hgl::graph::TextureAsset *asset)
        {
            if (asset == texture_asset && texture == nullptr) return;
            texture = nullptr;
            texture_asset = asset;
            ++texture_generation;
        }

        /// 清除覆盖，回退到配方默认纹理
        void ClearTextureOverride()
        {
            if (!texture && !texture_asset) return;
            texture = nullptr;
            texture_asset = nullptr;
            ++texture_generation;
        }
    };//class Sprite2DComponent

    /// 登记类型号（CPU-only：无 GPU 行、不写 EntityGPU.row[]）。
    template<> struct ComponentTypeOf<Sprite2DComponent> { static constexpr ComponentType value = ComponentType::Sprite2D; };
}//namespace hgl::ecs