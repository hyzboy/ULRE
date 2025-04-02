#pragma once

#include<hgl/graph/SceneNode.h>

namespace hgl::graph
{
    class CameraData
    {
    };

    class CameraManager
    {
    public:
    };

    /**
    * 世界场景管理器<Br>
    * 管理一个世界场景中的所有资源与场景节点
    */
    class SceneWorld
    {
        SceneNode *root_node;                           ///<世界根节点

    public:

        SceneWorld()
        {
            root_node=new SceneNode;
        }

        virtual ~SceneWorld()
        {
            SAFE_CLEAR(root_node);
        }

        SceneNode *GetRootNode(){return root_node;}
    };//class SceneWorld
}//namespace hgl::graph
