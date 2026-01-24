#pragma once

#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/Sky.h>
#include<hgl/io/event/EventDispatcher.h>

namespace hgl::graph
{
    class RenderFramework;

    using UBOSkyInfo=UBOInstance<SkyInfo>;

    /**
    * 世界管理器<Br>
    * 管理一个世界中的所有资源与世界节点
    */
    class World
    {
        RenderFramework *render_framework=nullptr;      ///<渲染框架

        IDString world_name;                            ///<世界名称

        SceneNode *root_node;                           ///<世界根节点

        OrderedValueSet<SceneNode *> all_nodes;               ///<世界内所有节点

    protected:

        DescriptorBinding * world_desc_binding  =nullptr;   ///<世界通用描述符绑定器(仅包含与世界相关的公用UBO，如天空等)

        UBOSkyInfo *        ubo_sky_info        =nullptr;   ///<天空信息UBO

    protected:  // event dispatcher

        io::EventDispatcher event_dispatcher;               ///<事件分发器

    public:

        const   IDString &  GetWorldName()const{return world_name;}                                 ///<获取世界名称

                SceneNode * GetRootNode (){return root_node;}                                       ///<获取世界根节点

        RenderFramework *   GetRenderFramework()const{return render_framework;}

        DescriptorBinding * GetDescriptorBinding()const{return world_desc_binding;}                 ///<获取世界描述符绑定器

    public:

        World(RenderFramework *rf);
        virtual ~World();

        io::EventDispatcher *GetEventDispatcher(){return &event_dispatcher;}                        ///<获取事件分发器

        template<typename T,typename ...ARGS>
        T *CreateNode(ARGS...args)
        {
            T *sn=new T(args...);

            if(!sn)
                return(nullptr);

            if(!root_node)
                root_node=sn;

            all_nodes.Add(sn);

            sn->OnChangeScene(this);
            return sn;
        }

        virtual void Tick(double){}                                                                 // 世界自身逐帧更新
    };//class World

    bool RegisterWorld(World *sw);                      ///<注册世界
    bool UnregisterWorld(const IDString &world_name);   ///<注销世界

    inline bool UnregisterWorld(World *sw)              ///<注销世界
    {
        if(!sw)return(false);

        return UnregisterWorld(sw->GetWorldName());
    }

    World *GetWorld(const IDString &world_name);        ///<获取指定名称的世界
}//namespace hgl::graph
