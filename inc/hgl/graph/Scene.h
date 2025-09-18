#pragma once

#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/Sky.h>
#include<hgl/io/event/EventDispatcher.h>

namespace hgl::graph
{
    class RenderFramework;

    class LineRenderManager;

    using UBOSkyInfo=UBOInstance<SkyInfo>;

    /**
    * 场景管理器<Br>
    * 管理一个场景中的所有资源与场景节点
    */
    class Scene
    {
        RenderFramework *render_framework=nullptr;      ///<渲染框架

        U8String SceneName;                             ///<场景名称

        ObjectList<SceneNode> SceneNodePool;            ///<场景节点池

        SceneNode *root_node;                           ///<场景根节点

        CameraControl *camera_control = nullptr;        ///<相机控制器

        LineRenderManager * line_render_manager =nullptr;

    protected:

        DescriptorBinding * descriptor_binding  =nullptr;   ///<场景通用描述符绑定器

        UBOSkyInfo *        ubo_sky_info        =nullptr;   ///<天空信息UBO

    protected:  // event dispatcher

        io::EventDispatcher event_dispatcher;           ///<事件分发器

    public:

        const   U8String &  GetSceneName()const{return SceneName;}              ///<获取场景名称

                SceneNode * GetRootNode (){return root_node;}                   ///<获取场景根节点

        RenderFramework *   GetRenderFramework()const{return render_framework;}

        CameraControl *     GetCameraControl()const{return camera_control;}

        LineRenderManager * GetLineRenderManager()const{return line_render_manager;}///<获取线段渲染管理器

    public:

        Scene(RenderFramework *rf);
        virtual ~Scene();

        void SetCameraControl(CameraControl *cc)
        {
            camera_control=cc;
        }

        io::EventDispatcher &GetEventDispatcher()  ///<获取事件分发器
        {
            return event_dispatcher;
        }

        DescriptorBinding *GetDescriptorBinding()const { return descriptor_binding; }///<获取场景通用描述符绑定器
    };//class Scene

    bool RegisterScene(Scene *sw);                      ///<注册场景
    bool UnregisterScene(const U8String &scene_name);   ///<注销场景

    inline bool UnregisterScene(Scene *sw)              ///<注销场景
    {
        if(!sw)return(false);

        return UnregisterScene(sw->GetSceneName());
    }

    Scene *GetScene(const U8String &scene_name);        ///<获取指定名称的场景
}//namespace hgl::graph
