#pragma once

#include<cstdint>

#include<hgl/common/DebugChannel.h>
#include<hgl/time/Time.h>

namespace hgl::ecs
{
    /**
     * 主帧 CPU 剖析槽。
     *
     * 与 `PhaseProbeSystem` 式「注册一个系统到某 phase」的探针不同：那些探针只能观测
     * **同 phase 内已注册系统之后**的时点，因此 `phase_n → phase_n+1` 的差值实际覆盖的
     * 是「phase_n 剩余系统 + phase_n+1 全部内容」，把阶段名当归属会系统性错配。
     *
     * 本文件的槽一律由引擎在**真实调用边界**（`BeginManagedRenderFrame` /
     * `PrepareRenderPassSetup` / `ExecuteRenderGraphPasses` / `EndManagedRenderFrame` /
     * 收集与合批函数内部）累加 ⇒ 各槽互不重叠，可直接相加与总帧时间对比。
     *
     * 口径：全部是 **CPU 墙钟**。`AcquireSwapchain` / `FrameSync` / `SubmitPresent`
     * 里含 CPU 等待 GPU 的时间（那是「本帧被 GPU 卡住多久」，不是「GPU 执行了多久」）。
     * 没有任何一处是 GPU 时间戳，不得当作 GPU 耗时。
     */
    enum class FrameProfileSlot : uint32_t
    {
        // ── 主帧装配（ContextRender.cpp / RenderGraph.cpp，真实调用边界）──────
        ScenePrePass = 0,   ///< ExecuteScenePrePassWorkflow（主光 CSM 阴影全流程）
        AcquireSwapchain,   ///< AcquireSwapchainImage（含 WSI 取图等待）
        PreBeginFrame,      ///< RenderPreBeginFrame + viewport 同步 + 纹理上传队列
        RenderCoreBegin,    ///< render_core->BeginFrame()
        TransformSubmit,    ///< TransformSystem::SubmitTransformUpdates()
        CameraPublish,      ///< CameraSystem::PublishCameraRows()
        CollectPhase,       ///< RunRenderPhaseUpdates(RenderCollect) 全段
        SyncWorldAddresses, ///< SyncWorldAddresses()
        BatchPhase,         ///< RunRenderPhaseUpdates(RenderBatch) 全段
        BufferCommit,       ///< RenderBufferCommit()
        BufferUpload,       ///< RenderBufferUpload()
        FrameSync,          ///< RenderFrameSync()（含 GPU 剔除派发与 BeginPass）
        BeginRenderPass,    ///< render_core->BeginRenderPass()
        DrawRecord,         ///< ExecuteRenderGraphPasses()（绘制命令录制）
        EndFrame,           ///< render_core->EndFrame()
        SubmitPresent,      ///< SubmitFrameToRenderTarget()（vkQueueSubmit + 呈现）

        // ── 收集系统内部（RenderPrimitiveCollectSystem::Update）───────────────
        CollectPreScan,     ///< 材质/管线预扫循环（决定本帧是否需要物化）
        CollectMainLoop,    ///< 主循环（策略判据 + item 装配 + 4-ID/事实发布）
        CollectPublish,     ///< 收集尾部（诊断/统计提交）

        // ── 合批内部（PrimitiveBatchPipeline）────────────────────────────────
        BatchBuildMaterial, ///< BuildMaterialBatches()
        BatchGPUCullGuard,  ///< TryApplyGPUCullOverride()
        BatchFinalize,      ///< FinalizeBatches()

        // ── 应用侧（示例自填）────────────────────────────────────────────────
        AppTick,            ///< 应用 Tick 整体（含 WorkObject::Tick 内的 ECS/Jolt）

        // ── 采样（必须与渲染分离计量）────────────────────────────────────────
        SampleReadback,     ///< SyncForParityReadback() + Map（采样帧特有的 GPU 同步）

        Count
    };

    /**
     * 主帧剖析**计数**槽。
     *
     * 只用于回答「本帧做了多少工作」，不是时间。与时间槽一样按窗口累加、由读取方重置。
     */
    enum class FrameProfileCount : uint32_t
    {
        CollectPrimitives = 0,  ///< 参与收集判据的 GeometryData 数（每 pass）
        CollectItems,           ///< 实际进入批的图元项数
        Batches,                ///< 材质批数
        BucketGroups,           ///< 几何批（聚簇单元，即 TryApplyGPUCullOverride 的 geometry_batches）数
        CollectCalls,           ///< 收集系统 Update 调用次数（多 pass ⇒ 每帧大于 1）
        Count
    };

    constexpr uint32_t kFrameProfileSlotCount  = static_cast<uint32_t>(FrameProfileSlot::Count);
    constexpr uint32_t kFrameProfileCountCount = static_cast<uint32_t>(FrameProfileCount::Count);

    /// 槽名（空表项返回 "?"；供报表打印，**不要**在每帧热路径调用）
    const char * GetFrameProfileSlotName(FrameProfileSlot slot);
    const char * GetFrameProfileCountName(FrameProfileCount slot);

    /**
     * 全局主帧剖析累加器。
     *
     * 设计约束：
     * - 关闭时 `Add` / `Count` 只付一次函数本地 static 的 bool 读，不做取时刻之外的任何工作。
     * - 默认开关 = `ULRE_DEBUG.toml` 的 `channels.perf`；应用可 `SetEnabled()` 显式覆盖
     *   （示例的 `CSTRESS_PROFILE=0` 走这条，保证「关」就是真关）。
     * - 累加是**窗口式**：读取方在打印后调 `Reset()`；不自行按帧清零，避免多 pass 世界被截断。
     * - 非线程安全：只允许主渲染线程写、读。
     */
    struct FrameProfiler
    {
        /// 显式开关覆盖。参数为 `true`/`false` 时永久覆盖（不再看 perf 通道）；
        /// 传 `SetEnabledOverride(false)` 之外的场景由应用自行决定，本接口不做引用计数。
        static void SetEnabled(bool on);

        /// 当前是否真的在计时
        static bool IsEnabled();

        /// 累加一段耗时（微秒）。关闭时立即返回。
        static void Add(FrameProfileSlot slot, uint64_t us);

        /// 累加计数。关闭时立即返回。
        static void Count(FrameProfileCount slot, uint64_t n = 1);

        static uint64_t GetUs(FrameProfileSlot slot);
        static uint64_t GetCount(FrameProfileCount slot);

        /// 全部时间与计数清零（读取方打印完一个窗口后调用）
        static void Reset();

        /// 时间槽之和（用于与总帧时间对比出「未归因」）
        static uint64_t SumUs();

        /// `Add` 被调用（计入样本）的次数基准：以 AppTick 之外的明确入口计数
        static uint64_t SampleCount();
        static void     CountSample();

        /// 被判定为无效（时间戳乱序 / 缺失）而丢弃的样本数
        static void     DropSample();
        static uint64_t DroppedSamples();
    };

    /// RAII 计时段：开关判定只做一次，析构时累加。
    struct FrameProfileSpan
    {
        FrameProfileSlot slot;
        bool             on;
        uint64_t         t0;

        explicit FrameProfileSpan(FrameProfileSlot s)
            : slot(s), on(FrameProfiler::IsEnabled()), t0(on ? GetUptimeUs() : 0) {}

        ~FrameProfileSpan()
        {
            if (on)
                FrameProfiler::Add(slot, GetUptimeUs() - t0);
        }

        FrameProfileSpan(const FrameProfileSpan &) = delete;
        FrameProfileSpan & operator=(const FrameProfileSpan &) = delete;
    };
}//namespace hgl::ecs
