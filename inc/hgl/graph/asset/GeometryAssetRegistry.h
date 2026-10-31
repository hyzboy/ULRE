#pragma once

#include <hgl/graph/asset/GeometryAsset.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferManager;
    class GeometryCreater;

    /**
     * 几何资产注册器 —— 句柄表的**唯一真值**（对标 `TextureManager` 的纹理句柄表）。
     *
     * 归属：挂在 `GraphicsContext` 上（设备级），生命周期与设备对齐。
     * 它比任何世界/材质都活得久，因此 `PrimitiveAsset` / `GeometryData` 里的
     * `GeometryAsset*` 都是裸观察指针，不存在「世界先死、表还指向它」的悬垂。
     *
     * 两件事：
     * 1. **去重**：同一 `.geo.toml` / `.geo` 文件或同名内置几何体只创建一个句柄
     *    （按 `GeometryAsset::GetResourceId()` 归并），于是也只有一次读盘、一份 GPU buffer。
     * 2. **所有权**：句柄由本表 `delete`，句柄再 `delete` 自己的 `Geometry*`。
     *    **不要**再把这些 `Geometry*` 交给 `GeometryManager::Add`。
     *
     * 线程约定：全部接口只在主线程使用（GPU 创建与文件提交都有线程亲和性）。
     */
    class GeometryAssetRegistry
    {
    private:

        VulkanDevice *  device          = nullptr;      ///<不拥有
        BufferManager * buffer_manager  = nullptr;      ///<不拥有，可为空

        GeometryCreater *shared_creater = nullptr;      ///<不拥有，可为空（共享顶点池）

        ValueArray<GeometryAsset *> assets;             ///<句柄表(拥有)

    private:

        GeometryAsset * FindByResourceId(const U8String &resource_id)const;

    public:

        GeometryAssetRegistry()=default;
        explicit GeometryAssetRegistry(VulkanDevice *dev,BufferManager *bm=nullptr);
        ~GeometryAssetRegistry();

        GeometryAssetRegistry(const GeometryAssetRegistry &)=delete;
        GeometryAssetRegistry &operator=(const GeometryAssetRegistry &)=delete;

    public:

        /// 绑定目标设备/缓冲管理器（`GraphicsContext::Initialize` 之后才拿得到）
        void SetTargets(VulkanDevice *dev,BufferManager *bm){device=dev;buffer_manager=bm;}
        VulkanDevice *  GetDevice       ()const{return device;}
        BufferManager * GetBufferManager()const{return buffer_manager;}

        /**
         * @brief 绑定共享顶点池创建器（`GeometryCreater(VertexDataManager*)` 那种）。
         *
         * 内置几何体默认走 `CreateGeometryFromSpec(device,bm,...)`，每个几何各建一套私有
         * VDM/缓冲。想让一批几何共享同一个 `VertexDataManager`（混合缓冲、合并批次等），
         * 就先把创建器绑上来，之后的 URI 创建会改走
         * `CreateGeometryFromSpec(creater,spec)` —— 此时**顶点布局由创建器决定**，
         * `format_override` 只影响句柄记录的 `GetVertexFormat()`。
         *
         * 生命周期：本表**不拥有**创建器。创建器必须比它所创建的几何活得久；
         * 卸载前调用 `SetSharedCreater(nullptr)` 只是解除绑定，不会回收已建的几何 ——
         * 顺序应当是先 `Clear()`（销毁几何），再销毁创建器。
         */
        void            SetSharedCreater(GeometryCreater *c){shared_creater=c;}
        GeometryCreater *GetSharedCreater()const{return shared_creater;}

    public:     // 装配入口

        /**
         * @brief 按 `.geo.toml` 或 `.geo` 路径取（无则建）句柄。
         *
         * - 路径以 `.toml` 结尾 ⇒ 解析规约后建句柄；
         * - 其它后缀 ⇒ 当成 `.geo` minipack 直接读盘。
         *
         * 同一个规范化路径只会得到一个句柄。
         */
        GeometryAsset * GetOrLoad(const OSString &filename,
                                  const GeometryVertexFormat *format_override=nullptr);

        /**
         * @brief 按内存里的规约建句柄（内置几何体走这条，创建即 `Resident`）。
         *
         * 去重键是 `GeometryAsset::GetResourceId()`：同名/同参数的内置请求复用同一个句柄。
         * 需要"每个实体一份独立几何"时用 `CreateUniqueFromSpec`。
         */
        GeometryAsset * GetOrCreateFromSpec(const GeometryAssetSpec &spec,
                                            const GeometryVertexFormat *format_override=nullptr);

        /// 强制新建（不查表），用于需要独立 GPU buffer 的场合
        GeometryAsset * CreateUniqueFromSpec(const GeometryAssetSpec &spec,
                                             const GeometryVertexFormat *format_override=nullptr);

        /**
         * @brief 按 `AssetURI` 取（无则建）句柄 —— 与纹理完全对称的定位入口。
         *
         * 接受的全部写法（类型段可省，省略时按 `AssetType::Geometry` 补齐）：
         *
         * ```
         * geo:builtin:cube            预设名（走 GeometryPresetLibrary）
         * geo:file:res/geo/x.geo.toml 规约文件或 .geo minipack
         * builtin:cube                「省略类型」形式，等价于第一行
         * file:res/geo/x.geo.toml     「省略类型」形式，等价于第二行
         * res/geo/x.geo.toml          裸路径（等价于 file:）
         * ```
         *
         * `builtin:` 走内存规约 ⇒ 创建即 `Resident`，没有流送窗口；
         * `file:` 走读盘 ⇒ 可能停在 `MetaOnly`/`Loading`，由渲染门禁拦截。
         *
         * 失败（预设不存在、规约非法、读盘失败）⇒ 返回 nullptr。
         */
        GeometryAsset * GetOrLoadURI(const OSString &uri,
                                     const GeometryVertexFormat *format_override=nullptr);

    public:     // 查询

        GeometryAsset * FindByUri(const U8String &resource_id)const{return FindByResourceId(resource_id);}

        /// 句柄表只读视图（流送系统逐帧扫描待办句柄用）
        const ValueArray<GeometryAsset *> & GetAssets()const{return assets;}

        /// 尚未就绪且未在途的句柄数（诊断用）
        int GetPendingCount()const;

        /// 已就绪句柄数（诊断用）
        int GetResidentCount()const;

    public:     // 生命周期

        /// 销毁全部句柄（含其 Geometries）。设备销毁前必须调用。
        void Clear();
    };//class GeometryAssetRegistry

    /**
     * @brief RAII 作用域：把共享顶点池创建器临时绑到注册器上，析构自动还原。
     *
     * 示例里创建器往往只是某个函数内的临时对象（`GeometryCreater pc(vdm);`），
     * 而注册器是设备级的，不能长期持有它。用本作用域包住「建几何」的那几行即可：
     *
     * ```cpp
     * GeometryCreater pc(vdm);
     * SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);
     * geom = GetGeometry("geo:builtin:cube");   // 写进共享顶点池
     * ```
     */
    class SharedCreaterScope
    {
    private:

        GeometryAssetRegistry * registry = nullptr;
        GeometryCreater *       previous = nullptr;

    public:

        SharedCreaterScope(GeometryAssetRegistry *reg,GeometryCreater *creater)
            :registry(reg)
            ,previous(reg?reg->GetSharedCreater():nullptr)
        {
            if(registry)
                registry->SetSharedCreater(creater);
        }

        ~SharedCreaterScope()
        {
            if(registry)
                registry->SetSharedCreater(previous);
        }

        SharedCreaterScope(const SharedCreaterScope &)=delete;
        SharedCreaterScope &operator=(const SharedCreaterScope &)=delete;
    };//class SharedCreaterScope
}//namespace hgl::graph
