#ifndef HGL_GRAPH_RENDER_FRAMEWORK_INCLUDE
#define HGL_GRAPH_RENDER_FRAMEWORK_INCLUDE

#include<hgl/graph/EventDispatcher.h>
#include<hgl/graph/Scene.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 渲染框架类
         * 负责整合事件调度和场景管理
         */
        class RenderFramework : public BaseEventDispatcher
        {
        protected:
            Scene* active_scene;                     ///<当前激活的场景

        public:
            RenderFramework();
            virtual ~RenderFramework();

            // 场景管理
            Scene* GetActiveScene() { return active_scene; }
            void SetActiveScene(Scene* scene);

            // 框架更新
            virtual void Update(float deltaTime);

            /**
             * 处理输入事件并分发到场景
             * @param event 输入事件
             * @return 是否有处理该事件
             */
            bool ProcessInputEvent(const device_input::InputEvent& event);

        protected:
            // 框架级别的事件处理
            bool HandleEvent(const device_input::InputEvent& event) override;
        };

        /**
         * 辅助函数：从现有应用框架中分发事件
         * 可以被现有的VulkanApplicationFramework或其他框架调用
         */
        bool DispatchFrameworkEvent(RenderFramework* framework, const device_input::InputEvent& event);
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_RENDER_FRAMEWORK_INCLUDE