#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/input/Event.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::device_input;

/**
 * 自定义场景节点，演示事件处理
 */
class TestSceneNode : public SceneNode
{
private:
    int node_id;

public:
    TestSceneNode(int id) : node_id(id) {}

protected:
    bool HandleEvent(const InputEvent& event) override
    {
        // 演示处理鼠标移动事件
        // 注意：这里只是演示，实际使用需要根据event的类型来处理
        std::cout << "Node " << node_id << " received event" << std::endl;
        
        // 调用父类方法以继续事件传播到子节点
        return SceneNode::HandleEvent(event);
    }
};

/**
 * 自定义场景，演示场景级别的事件处理
 */
class TestScene : public Scene
{
protected:
    bool HandleEvent(const InputEvent& event) override
    {
        std::cout << "Scene received event" << std::endl;
        
        // 调用父类方法以继续事件传播
        return Scene::HandleEvent(event);
    }
};

/**
 * 自定义渲染框架，演示框架级别的事件处理
 */
class TestRenderFramework : public RenderFramework
{
protected:
    bool HandleEvent(const InputEvent& event) override
    {
        std::cout << "RenderFramework received event" << std::endl;
        
        // 调用父类方法以继续事件传播
        return RenderFramework::HandleEvent(event);
    }
};

int main()
{
    std::cout << "Testing EventDispatcher architecture..." << std::endl;
    
    // 创建渲染框架
    TestRenderFramework framework;
    
    // 创建场景
    TestScene* scene = new TestScene();
    
    // 创建场景节点
    TestSceneNode* node1 = new TestSceneNode(1);
    TestSceneNode* node2 = new TestSceneNode(2);
    TestSceneNode* childNode = new TestSceneNode(3);
    
    // 构建场景图
    scene->AddSceneNode(node1);
    scene->AddSceneNode(node2);
    node1->SubNode.Add(childNode);  // node1的子节点
    
    // 将场景设置到框架
    framework.SetActiveScene(scene);
    
    // 创建一个测试事件
    InputEvent testEvent;
    // 模拟鼠标移动事件
    testEvent.mouse_move.x = 100;
    testEvent.mouse_move.y = 200;
    
    std::cout << "\nDispatching test event through the hierarchy:" << std::endl;
    std::cout << "RenderFramework -> Scene -> SceneNodes" << std::endl;
    
    // 分发事件
    bool handled = framework.ProcessInputEvent(testEvent);
    
    std::cout << "\nEvent handled: " << (handled ? "Yes" : "No") << std::endl;
    
    // 清理
    delete scene;  // Scene的析构函数会清理节点
    
    std::cout << "\nTest completed successfully!" << std::endl;
    
    return 0;
}