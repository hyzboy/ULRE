#pragma once

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
     * | `MetaOnly`   | 只有文件名（`GetMeta().source_path`），尚无显存资源 | `GetTexture()==nullptr` |
     * | `Loading`    | 已被 `TextureStreamingSystem` 接管，异步读取/上传在途 | `GetTexture()==nullptr` |
     * | `Resident`   | 纹理已可绑定 | `GetTexture()!=nullptr` |
     * | `LoadFailed` | 读取或上传失败（可重试） | `GetTexture()==nullptr` |
     *
     * 因此**不需要两套接口**：作者创建一个句柄（给文件名或给现成纹理），材质直接收下；
     * 是否已加载由 ECS 侧判断并自动驱动，加载完成后再让图元进入可渲染状态。
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

    public:     // 只读观察

        TextureManager * GetTextureManager()const{return tex_manager;}
        const OSString & GetFilename      ()const{return GetMeta().source_path;}
        Texture *        GetTexture       ()const{return texture;}
        bool             IsReady          ()const{return texture!=nullptr;}
        bool             GetAutoMipmaps   ()const{return auto_mipmaps;}

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
    };//class TextureAsset
}//namespace hgl::graph
