#pragma once

#include <hgl/graph/asset/GeometryAsset.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferManager;

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
}//namespace hgl::graph
