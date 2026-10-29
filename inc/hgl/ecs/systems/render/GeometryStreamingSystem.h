#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    class LoadedFileManager;

    namespace graph
    {
        class GeometryAsset;
        class GeometryAssetRegistry;
    }
}

namespace hgl::ecs
{
    /**
     * 几何体流式加载系统 —— 把「几何句柄」变成「可画几何」。
     *
     * 为什么需要它：作者写 `geometry = "res/geo/teapot.geo.toml"` 之后，图元资产里的
     * `GeometryAsset` 只有路径（`MetaOnly`），显存里什么都没有。谁来读盘、谁在主线程
     * 建 GPU buffer、谁决定图元何时可以渲染，这些**不能散落在每个示例里**（否则又会
     * 退化成手工两段式同步加载）。本系统是这条链路的唯一引擎侧驱动者。
     *
     * 与 `TextureStreamingSystem` 的分工：
     *
     * | | 纹理 | 几何 |
     * |---|------|------|
     * | 后台 | 单 IO 线程读盘 | 同（共用 `LoadedFileManager`，设备级串行） |
     * | 主线程 | 提交 GPU 上传任务（`TextureLoadCommitter`） | 直接解析 Minipack 建 buffer |
     * | 就绪标志 | 上传完成回调 | 本帧解析成功即 `CommitAsyncGeometry` |
     *
     * 内置几何体（`GetSpec().kind == Builtin`）**不经过本系统**：`GeometryAsset::SetupFromSpec`
     * 里已经同步建好，创建即 `Resident`。
     *
     * 工作集 = `GeometryAssetRegistry` 的句柄表，不另存订阅名单：
     * 「是文件来源 + 未就绪 + 未在途」就是待办集合，句柄表本身即唯一真值。
     * 因此本系统**完全不接触 ECS 组件/实体**，只是一台句柄状态机。
     *
     * 每帧三段，相位固定为 `ExecutionPhase::RenderPreBeginFrame`
     * （必须早于渲染收集，否则本帧就绪的几何要等下一帧才画得上）：
     *
     * ```
     * ① Update()
     *      LoadedFileManager::Pump()        ← 派发上一帧读盘完成的回调
     *        OnFileLoaded → 解析 Minipack → 建 GPU buffer → CommitAsyncGeometry
     *      扫描句柄表：待办句柄 ⇒ Submit() + MarkAsyncRequested(id)
     *      LoadedFileManager::Collect()     ← 回收已归零的文件引用
     * ```
     *
     * 就绪判定与"未就绪不渲染"不在这里：`PrimitiveState::CanRender` 每帧现算
     * `PrimitiveAsset::IsGeometryReady()`（无缓存），句柄转入 `Resident` 的同一帧
     * 就能被收集看到，所以无需任何失效通知。
     *
     * 失败语义：读取或解析失败 ⇒ 句柄停在 `LoadFailed`，本系统**不自动重试**
     * （避免坏文件每帧重读磁盘刷屏）；该图元持续不渲染，并留下 error 日志。
     * 需要重试时由调用方 `ClearAsyncRequest()` 把它放回待办集合。
     */
    class GeometryStreamingSystem final : public System
    {
        class StreamHandler;

    private:

        StreamHandler *handler = nullptr;                       ///<读盘回调接收者(活到最后一个回调)
        LoadedFileManager *load_manager = nullptr;
        graph::GeometryAssetRegistry *geometry_registry = nullptr;

    private:

        bool EnsureManagers();                                  ///<惰性抓取上下文里的管理器
        void AcquireOneFrame();                                 ///<提交本帧该发起的读取
        graph::GeometryAsset *FindAssetByRequest(uint64 request_id)const;

    public:

        GeometryStreamingSystem();
        ~GeometryStreamingSystem() override;

        void Update(float delta_time) override;
        void Shutdown() override;

        int GetPendingRequestCount()const;
    };//class GeometryStreamingSystem
}//namespace hgl::ecs
