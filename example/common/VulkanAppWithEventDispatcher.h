#ifndef HGL_VULKAN_APP_WITH_EVENT_DISPATCHER_INCLUDE
#define HGL_VULKAN_APP_WITH_EVENT_DISPATCHER_INCLUDE

#include<hgl/graph/RenderFramework.h>
#include<hgl/input/Event.h>

/**
 * 演示如何将新的EventDispatcher系统集成到现有的VulkanApplicationFramework中
 * 
 * 使用方法：
 * 1. 在VulkanApplicationFramework中添加RenderFramework成员
 * 2. 在事件处理方法中调用DispatchFrameworkEvent
 * 3. 创建Scene和SceneNode来构建场景图
 */

/*
// 在VulkanApplicationFramework中添加以下代码：

class VulkanApplicationFramework
{
    // ... 现有成员变量 ...

protected:
    hgl::graph::RenderFramework* render_framework = nullptr;

public:
    // 在构造函数或Init方法中初始化
    bool Init(int w, int h) override
    {
        // ... 现有初始化代码 ...
        
        // 初始化事件分发框架
        render_framework = new hgl::graph::RenderFramework();
        
        return true;
    }
    
    // 在析构函数中清理
    virtual ~VulkanApplicationFramework()
    {
        // ... 现有清理代码 ...
        
        if (render_framework)
        {
            delete render_framework;
            render_framework = nullptr;
        }
    }
    
    // 获取渲染框架
    hgl::graph::RenderFramework* GetRenderFramework() { return render_framework; }
    
    // 在现有的事件处理方法中添加事件分发
    // 例如，如果有OnKeyPress方法：
    void OnKeyPress(int key) 
    {
        // 创建InputEvent
        hgl::device_input::InputEvent event;
        event.key_push.key = key;
        
        // 分发事件到框架
        if (render_framework)
        {
            hgl::graph::DispatchFrameworkEvent(render_framework, event);
        }
        
        // ... 现有的键盘处理逻辑 ...
    }
    
    // 鼠标事件处理示例
    void OnMouseMove(int x, int y)
    {
        hgl::device_input::InputEvent event;
        event.mouse_move.x = x;
        event.mouse_move.y = y;
        
        if (render_framework)
        {
            hgl::graph::DispatchFrameworkEvent(render_framework, event);
        }
        
        // ... 现有的鼠标处理逻辑 ...
    }
    
    // 在Update或Draw方法中更新框架
    void Update(float deltaTime)
    {
        if (render_framework)
        {
            render_framework->Update(deltaTime);
        }
        
        // ... 现有的更新逻辑 ...
    }
};

// 使用示例：
void SetupSceneWithEventDispatcher(VulkanApplicationFramework* app)
{
    // 获取渲染框架
    hgl::graph::RenderFramework* framework = app->GetRenderFramework();
    if (!framework) return;
    
    // 创建场景
    hgl::graph::Scene* scene = new hgl::graph::Scene();
    
    // 创建场景节点
    hgl::graph::SceneNode* rootNode = scene->GetRootNode();
    hgl::graph::SceneNode* gameObject1 = rootNode->CreateSubNode();
    hgl::graph::SceneNode* gameObject2 = rootNode->CreateSubNode();
    
    // 设置场景到框架
    framework->SetActiveScene(scene);
    
    // 现在事件会自动从VulkanApplicationFramework分发到Scene和SceneNode
}
*/

#endif//HGL_VULKAN_APP_WITH_EVENT_DISPATCHER_INCLUDE