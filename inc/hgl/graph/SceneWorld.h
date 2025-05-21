#pragma once

#include<hgl/graph/SceneNode.h>
#include<hgl/type/Pool.h>

namespace hgl::graph
{
    /**
    * 世界场景管理器<Br>
    * 管理一个世界场景中的所有资源与场景节点
    */
    class SceneWorld
    {
        U8String WorldName;                             ///<世界名称

        ObjectList<SceneNode> SceneNodePool;            ///<场景节点池

        SceneNode *root_node;                           ///<世界根节点

    public:

        const   U8String &  GetWorldName()const{return WorldName;}              ///<获取世界名称

                SceneNode * GetRootNode (){return root_node;}                   ///<获取世界根节点

    public:

        SceneWorld()
        {
            root_node=new SceneNode;
        }

        virtual ~SceneWorld()
        {
            SAFE_CLEAR(root_node);
        }
    };//class SceneWorld

    bool RegistrySceneWorld(SceneWorld *sw);                ///<注册场景世界
    bool UnregistrySceneWorld(const U8String &world_name);  ///<注销场景世界

    inline bool UnregistrySceneWorld(SceneWorld *sw)        ///<注销场景世界
    {
        if(!sw)return(false);

        return UnregistrySceneWorld(sw->GetWorldName());
    }

    SceneWorld *GetSceneWorld(const U8String &world_name);  ///<获取指定名称的场景世界
}//namespace hgl::graph
