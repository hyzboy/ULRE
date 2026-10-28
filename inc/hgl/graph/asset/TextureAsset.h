#pragma once

#include<hgl/graph/asset/TextureURI.h>
#include<hgl/object/MetaObject.h>
#include<hgl/object/RefObject.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    class Texture;
    class TextureManager;

    /**
     * 纹理资源**统一句柄**（`MetaObject` 派生）。
     *
     * 为什么存在：材质系统原先有两条互不相干的入口 —— `SetTextureResource(name,Texture*,Sampler*)`
     * 要求调用方**先自己同步加载**，异步路径则只能先在别处读盘再回填。两者行为不一致，
     * 而且「文件还没读」这件事在材质侧无法表达。
     *
     * 现在材质只认这一个类型，句柄自身携带两种合法状态：
     *
     * | PayloadState | 含义 | 材质侧可见 |
     * |--------------|------|-----------|
     * | `MetaOnly`   | 只有来源（文件名 / RT 名），尚无显存资源 | `GetTexture()==nullptr` |
     * | `Loading`    | 已被 `TextureStreamingSystem` 接管，异步读取/上传在途 | `GetTexture()==nullptr` |
     * | `Resident`   | 纹理已可绑定 | `GetTexture()!=nullptr` |
     * | `LoadFailed` | 读取或上传失败（可重试） | `GetTexture()==nullptr` |
     *
     * `Resident` 只表示**容器**（VkImage/VkImageView）已存在，不等于**内容**已有意义：
     * - 文件来源：读盘 + 上传完成 = 内容就绪，两者同一时刻发生；
     * - RT 来源：附件纹理在 RT 创建瞬间就存在，但此刻还没画过任何东西。若把
     *   `Resident` 直接当成"可以采样"，第一帧就会把清屏色当内容采样，而且**永远**
     *   不会重画（下游认为已就绪）。因此 RT 句柄额外带一个内容就绪位：
     *   `BindRenderTargetTexture()` 只推 `Resident`（容器就绪），内容就绪由生产方
     *   （`OffscreenWorld` 完成一次有效绘制后）调用 `MarkContentReady()` 置位。
     *   `IsReady()` = 容器就绪 **且** 内容就绪 ⇒ `CanRender` 门禁对两者一视同仁。
     *
     * 因此**不需要两套接口**：作者创建一个句柄（给文件名、给 RT 名、或给现成纹理），材质直接收下；
     * 是否已加载由 ECS 侧判断并自动驱动，加载完成后再让图元进入可渲染状态。
     *
     * 来源有两种（`TextureURIKind`）：
     * - `File`（`file:<path>` 或裸路径）：由 `TextureStreamingSystem` 驱动异步读盘上传。
     * - `RenderTarget`（`rt:<target>[:<attachment>]`）：不读盘，由 `RenderTargetManager`
     *   在附件纹理创建/重建时自动绑定，销毁时自动解绑。这使离屏输出可以直接被材质引用，
     *   无需调用方手动 `GetColorTexture()` 再包装。
     *
     * 生命周期：句柄由 `TextureManager` 的句柄表**拥有并销毁**（`TextureManager` 比任何
     * 世界/材质都活得久），材质侧只持有裸观察指针 ⇒ 不存在引用计数竞态，也不会出现
     * 「材质先释放把句柄带走、后台表还指向它」的悬垂。
     */
    class TextureAsset : public hgl::MetaObject
    {
    public:

        HGL_OBJECT(TextureAsset,hgl::MetaObject)

    private:

        TextureManager *    tex_manager     = nullptr;      ///<纹理管理器(不拥有)
        Texture *           texture         = nullptr;      ///<Resident 状态下的实际纹理
        bool                auto_mipmaps    = false;        ///<源无 mip 链时是否自动生成
        bool                async_requested = false;        ///<异步读取已提交(防重复提交)
        uint64              async_request_id= 0;            ///<异步读取请求号(诊断用)

        TextureURIKind      source_kind     = TextureURIKind::File;
        OSString            rt_target;                      ///<RenderTarget 来源：目标名
        OSString            rt_attachment;                  ///<RenderTarget 来源：附件名
        bool                content_ready = false;          ///<内容就绪(RT:已按此目标画过一帧)

    protected:  // MetaObject 双层契约

        bool OnInitMeta() override;
        void OnReleaseMeta() override;
        bool OnLoadPayload() override;                      ///<同步兜底路径(工具/测试)
        void OnUnloadPayload() override;

    public:

        explicit TextureAsset(TextureManager *tm);
        ~TextureAsset() override = default;

    public:     // 构造入口（只在创建期调用一次）

        /** @brief 只给文件名：句柄停在 `MetaOnly`，等 ECS 侧驱动异步加载。 */
        bool SetupFromFilename(const OSString &filename,bool auto_mipmaps=false);

        /** @brief 给一张已就绪的纹理：句柄直接进入 `Resident`（包装语义，不接管所有权）。 */
        bool SetupFromTexture(Texture *tex);

        /**
         * @brief 绑定到某个 RenderTarget 的附件：句柄停在 `MetaOnly`，
         *        等 `RenderTargetManager` 创建附件后调用 `BindRenderTargetTexture` 转正。
         */
        bool SetupFromRenderTarget(const OSString &target,const OSString &attachment);

    public:     // 只读观察

        TextureManager * GetTextureManager()const{return tex_manager;}
        const OSString & GetFilename      ()const{return GetMeta().source_path;}
        Texture *        GetTexture       ()const{return texture;}
        bool             IsReady          ()const{return texture!=nullptr && content_ready;}
        bool             IsContentReady   ()const{return content_ready;}
        bool             GetAutoMipmaps   ()const{return auto_mipmaps;}

        TextureURIKind   GetSourceKind    ()const{return source_kind;}
        bool             IsRenderTarget   ()const{return source_kind==TextureURIKind::RenderTarget;}
        const OSString & GetRenderTargetName()const{return rt_target;}
        const OSString & GetRenderTargetAttachment()const{return rt_attachment;}

        /**
         * @brief 资源**身份**（材质配方绑定 / bindless 去重键的唯一来源）。
         *
         * | 来源 | 身份 | 为什么不换别的 |
         * |------|------|----------------|
         * | 文件 | `texfile:<path>` | **就绪前后同一值** ⇒ 异步加载完成不需要改写配方绑定；同一文件的两个句柄自然归并 |
         * | RT   | `rtex:<target>:<attachment>` | 附件名必须参与（同一 RT 的 color0 与 depth 是两张纹理）；不能用纹理 ID（RT 重建后 ID 会变） |
         * | 仅包装现成纹理 | 空串 | 没有来源可作身份，调用方退回 `texid:<GetID()>` |
         */
        U8String GetResourceId()const;

    public:     // 异步状态机（只由 TextureStreamingSystem 写）

        bool IsAsyncRequested()const{return async_requested;}
        uint64 GetAsyncRequestID()const{return async_request_id;}

        /// 标记已提交异步读取；`MetaOnly`/`LoadFailed` ⇒ `Loading`
        void MarkAsyncRequested(uint64 request_id);

        /// 撤销「已提交」标记（失败或取消）；`Loading` ⇒ `MetaOnly`
        void ClearAsyncRequest();

        /// 上传完成：句柄转入 `Resident`
        bool CommitAsyncTexture(Texture *tex);

        /// 读取/上传失败：句柄转入 `LoadFailed`
        void FailAsyncLoad(const char *reason);

    public:     // RenderTarget 绑定（只由 RenderTargetManager 写）

        /// 附件纹理已创建/重建：句柄转入 `Resident`
        bool BindRenderTargetTexture(Texture *tex);

        /// 附件纹理即将销毁：句柄退回 `MetaOnly`（保留句柄本身，等待重建）
        void UnbindRenderTargetTexture(const char *reason);

        /**
         * @brief 生产方声明「本句柄背后的内容已经画出来了」。
         *
         * 只对 RT 来源有意义：容器早就在，内容要靠离屏 pass 真正绘制一次。
         * 由 `OffscreenWorld` 在其内部图元全部就绪并完成一次绘制后调用。
         */
        void MarkContentReady();
    };//class TextureAsset
}//namespace hgl::graph
