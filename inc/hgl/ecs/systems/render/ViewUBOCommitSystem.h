#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class GraphicsContext;
    }

    namespace ecs
    {
        class CameraSystem;
        class RenderDescriptorBindingSystem;

        /**
         * ViewUBOCommitSystem - 视图三件套（camera/viewport/sky）固定写入点
         *
         * 契约：每个 RT/RenderPass 开始时（PrepareRenderPassSetup 内、
         * BeginRenderPass 之前的 RenderBufferCommit 阶段），把本 world 的
         * camera / viewport UBO 与所有已物化的环境 profile sky UBO
         * 无条件全量写入 GPU，不依赖任何脏标记。
         *
         * - ColorPalette 不在此列：内容基本静态，维持"变化时写一次"
         * - 材质 SSBO 仍走作者侧 Commit()
         * - 这些 UBO 均为 host-visible 持久映射，Update() 直写可见，
         *   无需经过 RenderBufferUploadSystem 的设备级 StagedBuffer 扫描
         */
        class ViewUBOCommitSystem : public System
        {
        public:

            ViewUBOCommitSystem(const std::string &name = "ViewUBOCommitSystem");
            ~ViewUBOCommitSystem() override = default;

            void Update(float deltaTime) override;
        };
    }//namespace ecs
}//namespace hgl
