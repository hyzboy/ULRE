#pragma once

#include<hgl/object/TickObject.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKRenderTarget.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class Camera;
        struct ViewportInfo;
        struct CameraInfo;
    }

    /**
    * 工作对象</p>
    *
    * 逻辑更新写在 Tick（每帧、渲染前）；渲染帧内的录制钩子是 OnRenderPass。
    * 两者时序与合同见 OnRenderPass 的注释——不要在 OnRenderPass 里改场景状态。
    */
    class WorkObject:public TickObject
    {
    protected:

        OBJECT_LOGGER

    private:

        ecs::ECSContext *world=nullptr;

        graph::RenderContext *render_context=nullptr;

        bool render_dirty=true;

    protected:

        // 以下数据在 ECS 模式下来自 ECSContext/GraphicsContext

    public:

        ecs::ECSContext *           GetECSContext       (){return world;}
        graph::RenderContext *      GetRenderContext    (){return render_context;}
        graph::GraphicsContext *    GetGraphicsContext  ()
        {
            if (render_context)
            {
                if (auto *gc = render_context->GetGraphicsContext())
                    return gc;
            }
            if (world)
                return world->GetGraphicsContext();
            return nullptr;
        }

        graph::VulkanDevice *       GetDevice           ()
        {
            if (auto *gc = GetGraphicsContext())
                return gc->GetDevice();
            if (world && world->GetGPUDevice())
                return world->GetGPUDevice();
            return nullptr;
        }
        template<typename T> T *    GetManager          ()
        {
            auto *gc = GetGraphicsContext();
            return gc ? gc->GetManager<T>() : nullptr;
        }

        const VkExtent2D *          GetExtent           ();
        const graph::ViewportInfo * GetViewportInfo     ()const;
        graph::Camera *             GetCamera           ();
        const graph::CameraInfo *   GetCameraInfo       ()const;

        const math::Vector2i *      GetMouseCoord       ()const;

        /// 设置清屏色。清屏色唯一权威在渲染目标上（RenderTargetDesc::clear_color），
        /// 本方法转发写入当前世界绑定的 RT——与离屏 RT 的 desc 声明同一存储。
        void SetClearColor(const Color4f &color);

    public:

        virtual const bool IsDestroy  ()const{return false;}   ///< 退出机制未实现，恒 false（见 WorkManager::Run）

        const   bool IsRenderDirty  ()const{return render_dirty;}
                void MarkRenderDirty(){render_dirty=true;}
            void ClearRenderDirty(){render_dirty=false;}

    protected:

        // 保护的默认构造函数，用于子类或框架初始化
        WorkObject() = default;

    public:

        virtual ~WorkObject()=default;

        /// 注入 ECS 世界（由 RunFramework / 外部事件循环驱动方在 Init() 之前调用）
        void SetECSContext(ecs::ECSContext *ctx);

        virtual bool Init()=0;

        virtual void Tick(double);

        /// 渲染帧内录制钩子：动态渲染 pass 已开（BeginRenderPass 之后）、
        /// ECS 系统绘制之前、命令缓冲录制中执行。
        ///
        /// 合同：current_render_cmd 有效，只准录制绘制命令（vkCmdDraw 等）；
        /// **不要在此修改场景状态**（transform/材质等）——那属于 Tick
        ///（TransformSystem 在本回调之后才提交变换，Tick 里改与本回调里改
        /// 效果同帧等价；放 Tick 语义正确且不占用录制时间）。
        /// 范本：example/Basic/SimpleMeshTriangle.cpp
        virtual void OnRenderPass(double delta_time) {}

        /// [[deprecated]] 旧名。语义同 OnRenderPass——历史上名字误导了大量
        /// 示例在渲染回调里写逻辑更新，新代码一律重写 OnRenderPass。
        virtual void Render(double delta_time) {}

    public:

        // Use RenderContext/GraphicsContext directly for resource creation.
    };//class WorkObject
}//namespace hgl
