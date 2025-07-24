#include<hgl/graph/RenderFramework.h>

namespace hgl
{
    namespace graph
    {
        RenderFramework::RenderFramework()
        {
            active_scene = nullptr;
        }

        RenderFramework::~RenderFramework()
        {
            // 注意：这里不删除scene，因为scene的生命周期可能由外部管理
            active_scene = nullptr;
        }

        void RenderFramework::SetActiveScene(Scene* scene)
        {
            active_scene = scene;
            
            // 将场景设置为事件监听器
            if (scene)
            {
                AddEventListener(scene);
            }
        }

        void RenderFramework::Update(float deltaTime)
        {
            if (active_scene)
            {
                active_scene->Update(deltaTime);
            }
        }

        bool RenderFramework::ProcessInputEvent(const device_input::InputEvent& event)
        {
            return DispatchEvent(event);
        }

        bool RenderFramework::HandleEvent(const device_input::InputEvent& event)
        {
            // 框架级别的事件处理
            // 例如：全局热键、系统级别的输入处理等
            
            // 目前不处理任何框架级别的事件，直接传递给场景
            return false;  // 允许事件继续传播到场景
        }

        // 辅助函数实现
        bool DispatchFrameworkEvent(RenderFramework* framework, const device_input::InputEvent& event)
        {
            if (framework)
            {
                return framework->ProcessInputEvent(event);
            }
            
            return false;
        }
    }//namespace graph
}//namespace hgl