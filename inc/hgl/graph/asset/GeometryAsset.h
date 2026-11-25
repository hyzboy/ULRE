#pragma once

#include <hgl/graph/asset/GeometryAssetSpec.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/object/MetaObject.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferManager;
    class Geometry;
    class GeometryAssetRegistry;

    /**
     * 几何资源**统一句柄**（`MetaObject` 派生，与 `TextureAsset` 同构）。
     *
     * 为什么存在：几何体此前有两条互不相干的入口 —— `LoadGeometry` 同步读盘解包，
     * 内置几何体则只能在 C++ 里手工拼 `InlineGeometry` 参数结构体。两者都无法表达
     * 「这个几何体还没就绪」这件事，于是渲染侧只能假设「拿到 `Geometry*` 就一定能画」。
     *
     * 现在渲染侧只认这一个类型，句柄自身携带两种合法就绪路径：
     *
     * | 就绪路径 | 触发点 | 是否读盘 |
     * |----------|--------|---------|
     * | 内置参数化 | `SetupFromSpec` 内**立即**建 GPU buffer，同一调用返回即 `Resident` | 否 |
     * | 外部模型文件 | `GeometryStreamingSystem` 后台读盘 → 主线程 `CommitAsyncGeometry` | 是 |
     *
     * 因此内置几何体**不经过**异步管线（纯内存计算，没有可等待的 IO），外部文件才需要
     * 等 `Resident`。渲染门禁对两者一视同仁：`IsReady()` 为真才允许进收集。
     *
     * 生命周期：句柄由 `GeometryAssetRegistry` 的表**拥有并销毁**。句柄反过来拥有自己
     * 创建的 `Geometry*`（`OnUnloadPayload` 里 delete），因此**不要再把这个 `Geometry*`
     * 塞进 `GeometryManager::Add`** —— 那会造成两条销毁路径。
     */
    class GeometryAsset : public hgl::MetaObject
    {
    public:

        HGL_OBJECT(GeometryAsset,hgl::MetaObject)

    private:

        GeometryAssetRegistry * registry        = nullptr;  ///<所属注册器(不拥有)
        VulkanDevice *          device          = nullptr;  ///<创建设备(不拥有)
        BufferManager *         buffer_manager  = nullptr;  ///<缓冲管理器(不拥有，可为空)

        GeometryAssetSpec       spec;                       ///<资产规约(内置参数 / 文件路径 / 顶点格式)
        GeometryVertexFormat    vertex_format;              ///<创建期确定的顶点布局

        /// 创建期实际生效的**外部**顶点格式覆盖摘要（0 = 无覆盖 / 覆盖不生效）。
        /// 只参与资源身份：同一 source 用不同覆盖格式请求时必须各自成一份几何，
        /// 否则后者会命中前者，顶点布局与 shader 期望不符。
        uint64                  format_override_hash = 0;

        Geometry *              geometry        = nullptr;  ///<Resident 状态下的实际几何体

        bool                    async_requested = false;    ///<异步读取已提交(防重复提交)
        uint64                  async_request_id= 0;        ///<异步读取请求号(诊断用)

    protected:  // MetaObject 双层契约

        bool OnInitMeta() override;
        void OnReleaseMeta() override;
        bool OnLoadPayload() override;                      ///<同步兜底路径(工具/测试)
        void OnUnloadPayload() override;

    public:

        GeometryAsset(GeometryAssetRegistry *reg,VulkanDevice *dev,BufferManager *bm);
        ~GeometryAsset() override;

    public:     // 构造入口

        /**
         * @brief 按规约装配句柄。
         *
         * 内置几何体（`kind == Builtin`）在本次调用内就完成 GPU 创建并转入 `Resident`；
         * 外部文件（`kind == File`）只记录路径，停在 `MetaOnly` 等流送系统驱动。
         * `format_override` 给空则按 `spec.NormalFormat()` 生成标准布局。
         */
        bool SetupFromSpec(const GeometryAssetSpec &spec,
                           const GeometryVertexFormat *format_override = nullptr);

        /**
         * @brief 直接给一个 `.geo` minipack 文件（不经过 `.geo.toml`）。
         *        句柄停在 `MetaOnly`，等流送系统读盘。
         */
        bool SetupFromMiniPackFile(const OSString &filename,
                                   const GeometryVertexFormat &format);

    public:     // 只读观察

        GeometryAssetRegistry * GetRegistry     ()const{return registry;}
        VulkanDevice *          GetDevice       ()const{return device;}
        BufferManager *         GetBufferManager()const{return buffer_manager;}

        const GeometryAssetSpec & GetSpec          ()const{return spec;}
        const GeometryVertexFormat & GetVertexFormat()const{return vertex_format;}
        Geometry *              GetGeometry     ()const{return geometry;}

        /// 几何是否**可画**（容器与内容同一时刻就绪，无 RT 那种"容器早就绪"的情况）
        bool                    IsReady         ()const{return geometry!=nullptr;}

        bool                    NeedsFileLoad   ()const{return spec.kind==GeometryAssetKind::File;}

    public:     // 异步状态机（只由 GeometryStreamingSystem 写）

        bool IsAsyncRequested()const{return async_requested;}
        uint64 GetAsyncRequestID()const{return async_request_id;}

        /// 标记已提交异步读取；`MetaOnly`/`LoadFailed` ⇒ `Loading`
        void MarkAsyncRequested(uint64 request_id);

        /// 撤销「已提交」标记（失败或取消）；`Loading` ⇒ `MetaOnly`
        void ClearAsyncRequest();

        /// 读取完成：句柄转入 `Resident`
        bool CommitAsyncGeometry(Geometry *geo);

        /// 读取/创建失败：句柄转入 `LoadFailed`
        void FailAsyncLoad(const char *reason);

    public:     // 身份

        /**
         * @brief 资源**身份**（注册器去重 / 诊断日志的唯一来源）。
         *
         * | 来源 | 身份 | 为什么 |
         * |------|------|--------|
         * | 内置 | `geo:@<name>` ，无名则用 `geo:<shape>:<参数摘要>` | 参数相同的内置请求自然归并 |
         * | 文件 | `geofile:<path>` | 就绪前后同一值 ⇒ 加载完成无需改写引用 |
         *
         * 创建期使用了外部顶点格式覆盖（且共享顶点池未接管布局）时追加
         * `:fmt=<hash>` —— 同一 source 的不同顶点布局必须是不同资产。
         */
        U8String GetResourceId()const;

        /// 外部顶点格式覆盖的身份后缀（`format_override == nullptr` 时为空）。
        /// 注册器建键与 `GetResourceId()` 必须用同一份实现，避免两处规则漂移。
        static U8String MakeFormatOverrideSuffix(const GeometryVertexFormat *format_override);
    };//class GeometryAsset
}//namespace hgl::graph
