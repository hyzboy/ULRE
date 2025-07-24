#include<hgl/graph/Scene.h>

namespace hgl
{
    namespace graph
    {
        Scene::Scene()
        {
            root_node = new SceneNode();
            active_camera = nullptr;
        }

        Scene::~Scene()
        {
            ClearSceneNodes();
            
            if (root_node)
            {
                delete root_node;
                root_node = nullptr;
            }
        }

        void Scene::SetRootNode(SceneNode* node)
        {
            if (root_node && root_node != node)
            {
                delete root_node;
            }
            
            root_node = node;
        }

        void Scene::AddSceneNode(SceneNode* node)
        {
            if (node)
            {
                scene_nodes.Add(node);
                
                // 将节点添加到根节点下
                if (root_node)
                {
                    root_node->SubNode.Add(node);
                }
            }
        }

        void Scene::RemoveSceneNode(SceneNode* node)
        {
            if (node)
            {
                scene_nodes.Delete(node);
                
                // 从根节点中移除
                if (root_node)
                {
                    root_node->SubNode.Delete(node);
                }
            }
        }

        void Scene::ClearSceneNodes()
        {
            scene_nodes.ClearData();
            
            if (root_node)
            {
                root_node->SubNode.ClearData();
            }
        }

        void Scene::Update(float deltaTime)
        {
            // 更新场景节点
            RefreshMatrix();
            
            // 子类可以覆盖此方法进行更多的更新逻辑
        }

        void Scene::RefreshMatrix()
        {
            if (root_node)
            {
                root_node->RefreshMatrix();
            }
        }

        bool Scene::HandleEvent(const device_input::InputEvent& event)
        {
            // 场景级别的事件处理
            // 目前不处理任何事件，只是转发给节点
            DispatchEventToNodes(event);
            
            return false;  // 允许事件继续传播
        }

        void Scene::DispatchEventToNodes(const device_input::InputEvent& event)
        {
            // 将事件分发给所有场景节点
            const int count = scene_nodes.GetCount();
            SceneNode** node_list = scene_nodes.GetData();
            
            for (int i = 0; i < count; i++)
            {
                // 调用节点的事件处理方法
                if (node_list[i]->ProcessEvent(event))
                {
                    // 如果某个节点处理了事件，可以选择停止传播
                    // 这里选择继续传播给其他节点
                }
            }
            
            // 同时也将事件传递给根节点
            if (root_node)
            {
                root_node->ProcessEvent(event);
            }
        }
    }//namespace graph
}//namespace hgl