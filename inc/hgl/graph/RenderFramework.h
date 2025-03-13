#pragma once

#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/RenderList.h>

VK_NAMESPACE_BEGIN

class FontSource;
class TileFont;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;

class RenderModule;

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    GPUDevice *             device              =nullptr;

    RenderResource *        render_resource     =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     rp_manager          =nullptr;

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;

    SwapchainModule *       sc_module           =nullptr;

public:

            Window *                GetWindow           ()const{return win;}
            GPUDevice *             GetDevice           ()const{return device;}
            VkDevice                GetVkDevice         ()const{return device->GetDevice();}
    const   GPUPhysicalDevice *     GetPhysicalDevice   ()const{return device->GetPhysicalDevice();}
            GPUDeviceAttribute *    GetDeviceAttribute  ()const{return device->GetDeviceAttribute();}

            RenderResource *        GetRenderResource   ()const{return render_resource;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return rp_manager;}
    TextureManager *        GetTextureManager       (){return tex_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}

    SwapchainModule *       GetSwapchainModule      (){return sc_module;}
    SwapchainRenderTarget * GetSwapchainRenderTarget(){return sc_module?sc_module->GetRenderTarget():nullptr;}

public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public:

    RenderList *CreateRenderList()
    {
        return(new RenderList(device));
    }

    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                     ///<创建只使用一种字符的Tile字符管理对象

public: // event

    virtual void OnResize(uint w,uint h);
    virtual void OnActive(bool);
    virtual void OnClose();

};//class RenderFramework

VK_NAMESPACE_END
