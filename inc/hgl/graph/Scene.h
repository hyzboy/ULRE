#ifndef HGL_GRAPH_SCENE_INCLUDE
#define HGL_GRAPH_SCENE_INCLUDE

#include<hgl/graph/EventDispatcher.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/type/List.h>

namespace hgl
{
    namespace graph
    {
        class Camera;  // 前向声明

namespace hgl
{
    namespace graph
    {
        /**
         * 场景类
         * 管理场景图和事件分发
         */
        class Scene : public BaseEventDispatcher
        {
        protected:
            SceneNode* root_node;                    ///<根节点
            Camera* active_camera;                   ///<当前激活的相机
            ObjectList<SceneNode> scene_nodes;       ///<场景节点列表

        public:
            Scene();
            virtual ~Scene();

            // 场景管理
            SceneNode* GetRootNode() { return root_node; }
            void SetRootNode(SceneNode* node);

            Camera* GetActiveCamera() { return active_camera; }
            void SetActiveCamera(Camera* camera) { active_camera = camera; }

            // 场景节点管理
            void AddSceneNode(SceneNode* node);
            void RemoveSceneNode(SceneNode* node);
            void ClearSceneNodes();

            // 场景更新
            virtual void Update(float deltaTime);
            virtual void RefreshMatrix();

        protected:
            // 事件处理
            bool HandleEvent(const device_input::InputEvent& event) override;
            
            // 将事件分发给场景节点
            bool DispatchEventToNodes(const device_input::InputEvent& event);
        };
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_SCENE_INCLUDE