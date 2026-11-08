#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/type/String.h>
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
     * 纹理来源有三种（都留空 = 用配方默认图）：
     *   · texture_uri   —— 纹理 URI 路径（推荐：全 ECS 异步流式加载，由流送系统在后台读盘与上传）；
     *   · texture_asset —— 资产句柄（可为 MetaOnly，自动流送）；
     *   · texture       —— 已就绪的裸纹理（归属 TextureManager，本组件不持所有权）。
     */
    class Sprite2DComponent : public ComponentRow
    {
    public:

        OSString                  texture_uri;               ///< 覆盖纹理 URI（推荐：全 ECS 异步流式加载），为空 = 不通过 URI 覆盖
        hgl::graph::Texture      *texture       = nullptr;   ///< 覆盖纹理（已就绪），null = 不覆盖
        hgl::graph::TextureAsset *texture_asset = nullptr;   ///< 覆盖纹理句柄（可 MetaOnly），null = 不覆盖

        /// 纹理授权代：每次 Set* 递增；同步系统据此跳过重复下发
        uint32_t texture_generation = 0;

        /// 最近一次已下发到材质数据的代（仅 Sprite2DSyncSystem 写）
        uint32_t synced_generation = 0;

    public:

        Sprite2DComponent() = default;

        ~Sprite2DComponent() = default;

        const OSString &GetTextureURI() const { return texture_uri; }
        hgl::graph::Texture *GetTexture() const { return texture; }
        hgl::graph::TextureAsset *GetTextureAsset() const { return texture_asset; }
        bool HasTextureOverride() const { return !texture_uri.IsEmpty() || texture != nullptr || texture_asset != nullptr; }

        /// 设置纹理 URI（推荐模式：全 ECS 异步流式加载）
        /// 传入文件路径或纹理 URI（如 OS_TEXT("res/image/PeachHouse.Tex2D")），
        /// 由 Sprite2DSyncSystem 自动向 TextureManager 申请资产句柄，
        /// 并由 TextureStreamingSystem 异步流式读盘并上传 GPU，就绪后自动呈现在画面上。
        void SetTextureURI(const OSString &uri)
        {
            if (texture_uri == uri) return;
            texture_uri = uri;
            texture = nullptr;
            texture_asset = nullptr;
            ++texture_generation;
        }

        /// 便捷重载：SetTexture(uri)
        void SetTexture(const OSString &uri)
        {
            SetTextureURI(uri);
        }

        /// 用已就绪纹理覆盖 base_color（传 null 视为清除意图，见类注释的清除限制）
        void SetTexture(hgl::graph::Texture *tex)
        {
            if (tex == texture && texture_asset == nullptr && texture_uri.IsEmpty()) return;
            texture_uri.Clear();
            texture = tex;
            texture_asset = nullptr;
            ++texture_generation;
        }

        /// 用资产句柄覆盖 base_color（MetaOnly 句柄自动流送；材质侧无移除入口，
        /// 清除/换图后已下发的覆盖在切图完成前保持）
        void SetTextureAsset(hgl::graph::TextureAsset *asset)
        {
            if (asset == texture_asset && texture == nullptr && texture_uri.IsEmpty()) return;
            texture_uri.Clear();
            texture = nullptr;
            texture_asset = asset;
            ++texture_generation;
        }

        /// 清除覆盖意图：递增代数以停止后续下发；已写入材质的覆盖无法回退
        /// （MaterialData 暂无纹理移除入口，限制已记在类注释）
        void ClearTextureOverride()
        {
            if (!texture && !texture_asset && texture_uri.IsEmpty()) return;
            texture_uri.Clear();
            texture = nullptr;
            texture_asset = nullptr;
            ++texture_generation;
        }
    };//class Sprite2DComponent

    /// 登记类型号（CPU-only：无 GPU 行、不写 EntityGPU.row[]）。
    template<> struct ComponentTypeOf<Sprite2DComponent> { static constexpr ComponentType value = ComponentType::Sprite2D; };
}//namespace hgl::ecs