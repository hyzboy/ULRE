#pragma once

#include<hgl/graph/SceneNode.h>
#include<hgl/type/Pool.h>

namespace hgl::graph
{
    /**
    * 场景管理器<Br>
    * 管理一个场景中的所有资源与场景节点
    */
    class Scene
    {
        U8String SceneName;                             ///<场景名称

        ObjectList<SceneNode> SceneNodePool;            ///<场景节点池

        SceneNode *root_node;                           ///<场景根节点

    public:

        const   U8String &  GetSceneName()const{return SceneName;}              ///<获取场景名称

                SceneNode * GetRootNode (){return root_node;}                   ///<获取场景根节点

    public:

        Scene()
        {
            root_node=new SceneNode;
        }

        virtual ~Scene()
        {
            SAFE_CLEAR(root_node);
        }
    };//class Scene

    bool RegistryScene(Scene *sw);                      ///<注册场景
    bool UnregistryScene(const U8String &world_name);   ///<注销场景

    inline bool UnregistryScene(Scene *sw)              ///<注销场景
    {
        if(!sw)return(false);

        return UnregistryScene(sw->GetSceneName());
    }

    Scene *GetScene(const U8String &world_name);        ///<获取指定名称的场景
}//namespace hgl::graph
