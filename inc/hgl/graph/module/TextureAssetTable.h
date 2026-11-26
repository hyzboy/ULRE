#pragma once

#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/asset/TextureURI.h>
#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph
{
    class TextureManager;
    class Texture;
    struct TextureArraySpec;

    /**
     * 纹理句柄表（`TextureManager` 私有）：**资产查找与 RenderTarget 绑定**域。
     *
     * 与底层纹理创建 / 上传彻底分开：本类不碰 VkImage、不建纹理、不提交 DMA，
     * 只维护「谁引用哪个文件 / 哪个 RT 附件」这张句柄表与去重规则。
     *
     * 三条去重口径（各自一份判据，互不混用）：
     * - **文件**：`MakeLoadedFileKey(path)` + `kind` + `auto_mipmaps`；
     * - **纹理数组**：规范化后的 URI 文本；
     * - **RenderTarget**：`(target, attachment)`，附件名缺省为 color。
     *
     * 线性扫描是有意选择：句柄只在材质装配期创建，规模几十到几百；换哈希表还要为
     * `OSString` 造一套哈希，收益不成比例。
     *
     * 所有权：句柄由本表拥有（裸指针 + 显式 delete）。`TextureManager` 必须比任何
     * 持有句柄观察指针的消费者活得久。
     */
    class TextureAssetTable
    {
    private:
        /// 非拥有回指：句柄构造需要它，且句柄的同步/异步加载要经它取纹理管理器接口
        TextureManager *owner = nullptr;

        ValueArray<TextureAsset *> assets;

    public:
        explicit TextureAssetTable(TextureManager *manager) : owner(manager) {}

        /// 全部句柄只读视图（流式系统逐帧扫描 `MetaOnly` 句柄用）
        const ValueArray<TextureAsset *> &GetAssets() const { return assets; }

        /**
         * 释放全部句柄：先按稳定快照逐个 `Release`，再统一 delete，最后清表。
         * 顺序不可换 —— 句柄的 `OnUnloadPayload` 必须跑在纹理对象仍然有效的时候。
         */
        void ReleaseAll();

        // ── 文件句柄（按文件去重；`rt:` / 数组 URI 会转到对应分支） ──
        TextureAsset *CreateFromFilename(const OSString &filename, bool auto_mipmaps = false);

        // ── 纹理数组句柄（按规范化 URI 去重；装配在创建期同步完成） ──
        TextureAsset *GetOrCreateTextureArray(const TextureURI &uri);

        // ── RenderTarget 句柄（按 target+attachment 去重） ──
        TextureAsset *FindRenderTarget(const OSString &target, const OSString &attachment) const;
        TextureAsset *GetOrCreateRenderTarget(const OSString &target, const OSString &attachment);

        /**
         * 把一张已就绪的纹理包装成句柄（**不入文件名去重表**：按一次性句柄处理）。
         * 仍归本表所有。
         */
        TextureAsset *WrapTexture(Texture *texture);

        // ── RenderTarget 生命周期通知（写者 = RenderTargetManager） ──

        /// 附件纹理已创建/重建：绑定到所有相关句柄。返回绑定的句柄数。
        int BindRenderTarget(const OSString &target, Texture *texture, const OSString &attachment);

        /// 附件纹理即将销毁：把所有相关句柄退回 `MetaOnly`
        void UnbindRenderTarget(const OSString &target);

        /**
         * 生产方声明「这个 RenderTarget 的内容已经画出来了」：把该目标下所有句柄的内容
         * 就绪位置位。返回新置位的句柄数（0 = 调用方还没建过句柄，不是错误）。幂等。
         */
        int MarkRenderTargetContentReady(const OSString &target);
    };
}