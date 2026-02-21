#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/color/Color4f.h>

namespace hgl {
    namespace graph {
        class RenderCmdBuffer;
        class IRenderTarget;
        class RenderContext;
    }
}

namespace hgl::ecs
{
    /**
     * Legacy note (Phase 1):
     * - This type is kept for compatibility/documentation only.
     * - Default runtime frame driver is ECSContext::Render(float) + RenderSystemCore.
     * - Do not register RenderFrameSystem in new code paths.
     */
    /**
     * RenderFrameSystem: 渲染帧驱动系统
     *
     * 职责:
     * - 作为整个渲染流程的主驱动和协调器
     * - 管理每帧的渲染命令缓冲区
    * - 协调 RenderCollect、RenderBatch、RenderDrawSubmit、RenderSubmit 等子系统
     * - 处理渲染目标的帧同步
     *
     * 执行流程:
     * ```
     * ┌──────────────────────────────────────────┐
     * │ ECSContext::Tick(deltaTime)              │
     * │  └─ Tick所有系统                         │
     * └──────────────────────────────────────────┘
     *                    ↓
     * ┌──────────────────────────────────────────┐
     * │ ECSContext::Render(cmd, deltaTime)       │
    * │  ├─ RenderSwapchainNextImage             │
    * │  │   (获取 swapchain 图像)               │
    * │  ├─ RenderPreBeginFrame                  │
    * │  │   (同步父/子世界等准备工作)           │
    * │  ├─ RenderBeginFrame                     │
    * │  │   (开始新帧: RenderFrameSystem)       │
    * │  │   └─ 创建帧命令缓冲区                │
     * │  ├─ RenderPostBeginFrame                 │
     * │  │   (帧初始化完成)                      │
     * │  ├─ RenderCollect                        │
     * │  │   └─ RenderPrimitiveCollectSystem     │
     * │  │       (收集需要渲染的图元)            │
    * │  ├─ RenderBatch                          │
    * │  │   └─ RenderPrimitiveBatch* Systems    │
    * │  │       (Cull/Sort/BatchBuild/Finalize) │
    * │  ├─ RenderDrawSubmit                     │
    * │  │   ├─ RenderPrimitiveSubmitSystem      │
    * │  │   │   (发送绘制命令)                  │
    * │  │   └─ TextRenderSubmitSystem           │
    * │  │       (发送文本绘制命令)              │
    * │  ├─ RenderPostProcess                    │
    * │  │   └─ LineRenderSystem                 │
    * └──────────────────────────────────────────┘
    *
    * RenderSubmit 是帧驱动层的最终提交步骤，
    * 不在单次 RT/FBO 的 RenderStage 流程内执行。
     * └──────────────────────────────────────────┘
     * ```
     *
     * 设计原则:
     * 1. 单一职责: 只管理帧级别的命令缓冲区和渲染目标
     * 2. 通过钩子: 具体绘制工作由子系统完成
     * 3. 显式协调: 清晰的依赖和执行顺序
     * 4. 可插拔: 子系统可以独立添加/禁用
     *
     * 使用示例:
     * ```cpp
    * // Legacy only: retained for compatibility; not used by default runtime.
    * // Preferred path:
    * // ecs_context->Render(delta_time);
     *
    * // Frame lifecycle is driven by RenderSystemCore internally.
     * ```
     */
    class RenderFrameSystem : public System
    {
    protected:
        // 渲染资源
        graph::RenderContext* render_context = nullptr;
        graph::IRenderTarget* render_target = nullptr;
        graph::RenderCmdBuffer* current_cmd_buf = nullptr;

        // 渲染状态
        hgl::Color4f clear_color{0,0,0,1};
        bool frame_started = false;
        uint32_t frame_index = 0;

    public:
        explicit RenderFrameSystem(const std::string& name = "RenderFrame");
        virtual ~RenderFrameSystem() = default;

    public:
        // ===== 配置 =====

        /**
         * 设置渲染目标
         * @param rt 目标渲染目标
         */
        void SetRenderTarget(graph::IRenderTarget* rt) {
            render_target = rt;
        }

        /**
         * 获取当前渲染目标
         * @return 当前渲染目标指针
         */
        graph::IRenderTarget* GetRenderTarget() const {
            return render_target;
        }

        /**
         * 设置渲染上下文
         * @param ctx 渲染执行上下文
         */
        void SetRenderContext(graph::RenderContext* ctx) {
            render_context = ctx;
        }

        /**
         * 获取渲染上下文
         * @return 渲染执行上下文指针
         */
        graph::RenderContext* GetRenderContext() const {
            return render_context;
        }

        /**
         * 设置清屏颜色
         * @param color 清屏颜色
         */
        void SetClearColor(const hgl::Color4f& color) {
            clear_color = color;
        }

        /**
         * 获取清屏颜色
         * @return 清屏颜色
         */
        const hgl::Color4f& GetClearColor() const {
            return clear_color;
        }

    public:
        // ===== 帧级别生命周期 =====

        /**
         * 开始帧
         * - 获取 swapchain 图像
         * - 创建帧命令缓冲区
         * - 准备渲染资源
         */
        void BeginFrame(float deltaTime);

        /**
         * 系统更新（由 ECS 调用）
         * - 实际执行 RenderPass
         * - 调用其他子系统进行渲染
         */
        void Update(float deltaTime) override;

        /**
         * 结束帧
         * - 提交命令缓冲区
         * - Present swapchain
         * - 同步 GPU/CPU
         */
        void EndFrame(float deltaTime);

    public:
        // ===== 查询接口 =====

        /**
         * 获取当前帧的命令缓冲区
         * @return 当前渲染命令缓冲区
         */
        graph::RenderCmdBuffer* GetCurrentRenderCmdBuffer() const {
            return current_cmd_buf;
        }

        /**
         * 获取当前帧索引
         * @return 帧索引
         */
        uint32_t GetFrameIndex() const {
            return frame_index;
        }

        /**
         * 帧是否已启动
         * @return true 如果帧已启动，false 否则
         */
        bool IsFrameStarted() const {
            return frame_started;
        }

    protected:
        // ===== 内部实现 =====

        /**
         * 处理帧同步（虚函数，子类可覆盖）
         * - 等待 GPU 完成上一帧
         * - 处理类型缓冲区轮转
         */
        virtual void HandleFrameSync(float deltaTime);

        /**
         * 执行 RenderPass 设置（虚函数，子类可覆盖）
         * - 开始 RenderPass
         * - 绑定渲染管线
         * - 设置视口和剪裁矩形
         */
        virtual void ExecuteRenderPass(float deltaTime);

        /**
         * 清理渲染资源（虚函数，子类可覆盖）
         * - 重置命令缓冲区
         * - 清除渲硬件状态
         */
        virtual void CleanupFrame(float deltaTime);

    }; // class RenderFrameSystem

} // namespace hgl::ecs
