#pragma once

#include <hgl/ecs/core/system/System.h>
#include <hgl/ecs/core/render/RenderPassRequest.h>
#include <hgl/ecs/core/render/RenderFrameCache.h>
#include <hgl/ecs/core/render/ScenePipelineMode.h>
#include <hgl/ecs/core/RenderPassScopeState.h>
#include <hgl/ecs/core/MainFrameTopologyState.h>
#include <hgl/ecs/core/AdaptiveRenderGraphCache.h>
#include <hgl/type/Smart.h>
#include <hgl/type/ValueArray.h>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
        class RenderCmdBuffer;
        struct RenderPassOptions;
    }
}

namespace hgl::ecs
{
    class ECSContext;
    class System;
    class RenderSystemCore;

    /**
     * WorldRenderRunner —— 渲染执行器。
     *
     * 职责：
     * - 渲染帧生命周期管理（BeginManagedRenderFrame / EndManagedRenderFrame）
     * - 离屏渲染与 Pass 调度（RenderTo / RenderDrawOnly）
     * - 渲染管线阶段驱动（RunRenderPhaseUpdates / RunRenderSystemsInRange）
     * - 渲染帧缓冲提交与上传协调（PrepareRenderPassSetup / SyncRenderTargetViewportToExtent）
     * - 拓扑状态与自适应渲染图调度
     */
    class WorldRenderRunner
    {
    private:
        ECSContext *context = nullptr;

        AutoDelete<RenderSystemCore> render_core;
        RenderFrameCache render_frame_cache;
        uint32_t frame_index = 0;
        uint64_t render_submission_serial = 0;
        graph::RenderCmdBuffer *current_render_cmd = nullptr;

        MainFrameTopologyState main_frame_topology;
        RenderPassScopeState pass_scope;
        ScenePipelineMode scene_pipeline_mode = ScenePipelineMode::StandardLitCSM;
        mutable AdaptiveRenderGraphCache adaptive_graph_cache;

    public:
        explicit WorldRenderRunner(ECSContext *ctx = nullptr);
        ~WorldRenderRunner();

        uint32_t GetFrameIndex() const { return frame_index; }
        void SetFrameIndex(uint32_t index) { frame_index = index; }

        uint64_t GetRenderSubmissionSerial() const { return render_submission_serial; }
        void SetRenderSubmissionSerial(uint64_t s) { render_submission_serial = s; }
        uint64_t AdvanceRenderSubmissionSerial() { return ++render_submission_serial; }

        graph::RenderCmdBuffer *GetCurrentRenderCmd() const { return current_render_cmd; }
        void SetCurrentRenderCmd(graph::RenderCmdBuffer *cmd) { current_render_cmd = cmd; }

        RenderPassScopeState &PassScope() { return pass_scope; }
        const RenderPassScopeState &PassScope() const { return pass_scope; }

        RenderFrameCache &FrameCache() { return render_frame_cache; }
        const RenderFrameCache &FrameCache() const { return render_frame_cache; }

        MainFrameTopologyState &GetMainFrameTopology() { return main_frame_topology; }
        const MainFrameTopologyState &GetMainFrameTopology() const { return main_frame_topology; }

        AdaptiveRenderGraphCache &GetAdaptiveGraphCache() const { return adaptive_graph_cache; }
        void MarkSceneStructureDirty() const { adaptive_graph_cache.MarkDirty(); }

        void SetScenePipelineMode(ScenePipelineMode mode);
        ScenePipelineMode GetScenePipelineMode() const { return scene_pipeline_mode; }

        RenderSystemCore *GetRenderCore() const { return const_cast<RenderSystemCore *>((const RenderSystemCore *)render_core); }
        bool EnsureRenderCoreInitialized();
        void ResetRenderCore() { render_core = nullptr; }

        void ClearFrameCache();
        void ClearMaterialBatches();
        void Shutdown();

        // ── 渲染事件与响应 ──
        void OnSwapchainTargetChanged(graph::IRenderTarget *target);
        void OnRenderTargetResized(graph::IRenderTarget *target, uint32 width, uint32 height);
        void OnResize(const VkExtent2D &extent);

        // ── 渲染能力与预处理 ──
        bool HasPendingRenderableResources() const;
        void ExecuteScenePrePassWorkflow(float deltaTime);

        // ── 托管渲染帧执行 ──
        bool BeginManagedRenderFrame(float deltaTime, bool need_swapchain_acquire = true, const graph::RenderPassOptions *options = nullptr);
        bool EndManagedRenderFrame(float deltaTime);

        void RecordPreparedRenderPhaseRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime, bool submit_transforms = true, const char *log_prefix = nullptr);
        void RenderDrawOnly(graph::RenderCmdBuffer *cmd, float deltaTime);

        // ── 离屏渲染与主渲染入口 ──
        bool RenderTo(const RenderPassRequest &req);
        bool RenderTo(graph::IRenderTarget *rt, const hgl::Color4f &clear, float deltaTime, CullMode cull_mode = CullMode::Inherit);

        // ── 阶段细节执行 ──
        void RenderPreBeginFrame(float deltaTime);
        void RenderSwapchainNextImage(float deltaTime);
        bool AcquireSwapchainImage(float deltaTime);
        void SyncRenderTargetViewportToExtent();
        void RenderBufferCommit(float deltaTime);
        void RenderBufferUpload(float deltaTime);
        void RenderFrameSync(float deltaTime);
        void PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime);
        void RenderSubmit(float deltaTime);
        bool SubmitFrameToRenderTarget(float deltaTime);
        bool RecreateSwapchainIfNeeded();

        // ── 系统更新与调度 ──
        void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
        void RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
        void RunSystemUpdate(System *system, float deltaTime);
        void RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime);
    };
} // namespace hgl::ecs
