#pragma once

#include<hgl/platform/Window.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/module/GraphModuleManager.h>

VK_NAMESPACE_BEGIN

class FontSource;
class TileFont;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;
class SwapchainModule;

class RenderModule;

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    GPUDevice *             device              =nullptr;

    RenderResource *        render_resource     =nullptr;

private:

    double                  last_time           =0;
    double                  cur_time            =0;
    int64                   frame_count         =0;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     render_pass_manager =nullptr;

    TextureManager *        texture_manager     =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;

    SwapchainModule *       swapchain_module    =nullptr;

public:

    Window *                GetWindow           (){return win;}
    GPUDevice *             GetDevice           (){return device;}    
    GPUDeviceAttribute *    GetDeviceAttribute  (){return device->GetDeviceAttribute();}

    RenderResource *        GetRenderResource   (){return render_resource;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return render_pass_manager;}
    TextureManager *        GetTextureManager       (){return texture_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}

    SwapchainModule *       GetSwapchainModule      (){return swapchain_module;}

public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public:

    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                     ///<创建只使用一种字符的Tile字符管理对象

public: // event

    virtual void OnResize(uint w,uint h);
    virtual void OnActive(bool);
    virtual void OnClose();

protected:

    virtual void BeginFrame();
    virtual void EndFrame();

    virtual bool RunFrame(RenderModule *);

public:

    virtual bool Run(RenderModule *);
};//class RenderFramework

VK_NAMESPACE_END
