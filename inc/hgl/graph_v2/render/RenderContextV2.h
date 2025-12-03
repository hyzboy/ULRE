#pragma once

#include<hgl/graph_v2/scene/SceneContext.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

/**
 * 增强的渲染上下文 (Enhanced Render Context)
 * 
 * 设计目标 (Design Goals):
 * 1. 显式传递 - 作为参数显式传递，不通过 EventDispatcher 链获取
 * 2. 临时状态 - 只在渲染期间有效，不持有长期引用
 * 3. 集中管理 - 包含渲染所需的所有上下文信息
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: 通过 SceneNode::GetRenderContext() 的5层调用链获取
 * - 新系统: 作为参数显式传递给渲染函数
 * 
 * 使用示例 (Usage Example):
 * ```cpp
 * void SceneRenderer::RenderFrame()
 * {
 *     RenderContextV2 ctx;
 *     ctx.world = GetWorld();
 *     ctx.camera = GetCamera();
 *     // ... 设置其他上下文
 *     
 *     root_node->Render(&ctx);  // 显式传递
 * }
 * ```
 */

namespace hgl::graph_v2
{
    using namespace hgl::graph;

    /**
     * 渲染上下文
     * 
     * 命名说明: 使用 RenderContextV2 避免与旧系统的 RenderContext 冲突
     * 
     * 实现细节 (Implementation Details):
     * - 只包含指针，不拥有任何对象
     * - 轻量级，可以在栈上创建
     * - 在渲染函数间显式传递
     */
    class RenderContextV2
    {
    public:
        // ===== 场景相关 =====
        ISceneContext* scene_context = nullptr;     ///< 场景上下文
        
        // ===== 相机相关 =====
        Camera* camera = nullptr;                   ///< 当前相机
        const CameraInfo* camera_info = nullptr;    ///< 相机信息（缓存）
        
        // ===== 视口相关 =====
        const ViewportInfo* viewport = nullptr;     ///< 视口信息
        VkExtent2D extent = {0, 0};                 ///< 渲染区域大小
        
        // ===== 渲染器相关 =====
        LineRenderManager* line_render_mgr = nullptr;   ///< 线条渲染管理器
        
        // ===== 时间相关 =====
        double delta_time = 0.0;                    ///< 帧时间增量
        double total_time = 0.0;                    ///< 总运行时间

    public:
        /**
         * 构造函数
         */
        RenderContextV2() = default;

        /**
         * 检查上下文是否有效
         * @return 有效返回 true
         * 
         * 说明: 检查必要的字段是否已设置
         */
        bool IsValid() const
        {
            return scene_context != nullptr && camera != nullptr;
        }

        /**
         * 从场景上下文获取根节点
         * 便捷方法
         */
        ITransformNode* GetRootNode() const
        {
            return scene_context ? scene_context->GetRootNode() : nullptr;
        }

        /**
         * 更新相机信息缓存
         * 说明: 在渲染前调用，避免重复获取
         */
        void UpdateCameraInfo()
        {
            if (camera)
                camera_info = camera->GetCameraInfo();
        }

        /**
         * 重置上下文
         * 说明: 清空所有指针，准备下一次使用
         */
        void Reset()
        {
            scene_context = nullptr;
            camera = nullptr;
            camera_info = nullptr;
            viewport = nullptr;
            extent = {0, 0};
            line_render_mgr = nullptr;
            delta_time = 0.0;
        }
    };

    /**
     * 可渲染接口 (Renderable Interface)
     * 
     * 说明:
     * - 任何可以渲染的对象都应实现这个接口
     * - 接收 RenderContextV2 作为参数
     * - 不再通过复杂调用链获取上下文
     */
    class IRenderable
    {
    public:
        virtual ~IRenderable() = default;

        /**
         * 渲染
         * @param ctx 渲染上下文
         * 
         * 说明: 
         * - ctx 保证不为 nullptr
         * - ctx 在整个渲染过程中保持有效
         */
        virtual void Render(RenderContextV2* ctx) = 0;
    };

}//namespace hgl::graph_v2
