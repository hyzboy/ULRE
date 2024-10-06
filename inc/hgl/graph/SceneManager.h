#pragma once
#include<hgl/graph/SceneNode.h>

namespace hgl
{
    namespace graph
    {
        /**
        * 场景管理器<Br>
        * 管理一个场景中的所有资源与场景节点
        */
        class SceneManager
        {
            SceneNode *root_node;



        public:

                  SceneNode *GetSceneRoot()     {return root_node;}
            const SceneNode *GetSceneRoot()const{return root_node;}

            const uint GetNodeCount()const { return node_list.GetCount(); }

        public:



        };//class SceneManager
    }//namespace graph
}//namespace hgl
