#pragma once

#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/RenderList.h>
#include<hgl/graph/CameraControl.h>

VK_NAMESPACE_BEGIN

class FontSource;
class TileFont;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;

class RenderModule;

class Scene;
class Renderer;

class CameraComponentManager{/*现阶段测试使用*/};
class LightComponentManager{/*现阶段测试使用*/};

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    VulkanDevice *          device              =nullptr;

    RenderResource *        render_resource     =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     rp_manager          =nullptr;

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;

    SwapchainModule *       sc_module           =nullptr;

protected:

    CameraComponentManager *camera_component_manager=nullptr;
    LightComponentManager  *light_component_manager =nullptr;

protected:  //RenderContext,未来合并成一个RenderContext结构

    Scene *                 default_scene           =nullptr;
    Camera *                default_camera          =nullptr;
    CameraControl *         default_camera_control  =nullptr;
    Renderer *              default_renderer        =nullptr;

    void CreateDefaultRenderer();

public:

            Window *            GetWindow           ()const{return win;}
            VulkanDevice *      GetDevice           ()const{return device;}
            VkDevice            GetVkDevice         ()const{return device->GetDevice();}
    const   VulkanPhyDevice *   GetPhyDevice        ()const{return device->GetPhyDevice();}
            VulkanDevAttr *     GetDevAttr          ()const{return device->GetDevAttr();}

            RenderResource *    GetRenderResource   ()const{return render_resource;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return rp_manager;}
    TextureManager *        GetTextureManager       (){return tex_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}

    SwapchainModule *       GetSwapchainModule      (){return sc_module;}
    SwapchainRenderTarget * GetSwapchainRenderTarget(){return sc_module?sc_module->GetRenderTarget():nullptr;}

public:

    Scene *                 GetDefaultScene         (){return default_scene;}
    Camera *                GetDefaultCamera        (){return default_camera;}
    CameraControl *         GetDefaultCameraControl (){return default_camera_control;}
    Renderer *              GetDefaultRenderer      (){return default_renderer;}

public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public: // event

    virtual void OnResize(uint w,uint h);
    virtual void OnActive(bool);
    virtual void OnClose();

public: // other

    RenderList *CreateRenderList()
    {
        return(new RenderList(device));
    }

    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                     ///<创建只使用一种字符的Tile字符管理对象

public: // ComponentManager

    template<typename T> T *GetComponentManager()
    {
        return COMPONENT_NAMESPACE::GetComponentManager<T>(true);
    }

    template<> CameraComponentManager *GetComponentManager<CameraComponentManager>()
    {
        return camera_component_manager;
    }

    template<> LightComponentManager *GetComponentManager<LightComponentManager>()
    {
        return light_component_manager;
    }
};//class RenderFramework

VK_NAMESPACE_END
