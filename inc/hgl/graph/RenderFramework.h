#pragma once

#include<hgl/graph/VK.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/module/GraphModuleManager.h>

VK_NAMESPACE_BEGIN

class FontSource;
class TileFont;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;
class SwapchainModule;

class RenderFramework:public io::WindowEvent
{
    OSString            app_name;

    Window *            win                 =nullptr;
    VulkanInstance *    inst                =nullptr;

    GPUDevice *         device              =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     render_pass_manager =nullptr;
    RenderPass *            device_render_pass  =nullptr;

    TextureManager *        texture_manager     =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;

    SwapchainModule *       swapchain_module    =nullptr;

public:

    Window *            GetWindow       (){return win;}
    GPUDevice *         GetDevice       (){return device;}

    RenderPass *        GetRenderPass   (){return device_render_pass;}

public:

    GraphModuleManager *    GetModuleManager(){return module_manager;}

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

    void OnResize(uint w,uint h);
    void OnActive(bool);
    void OnClose();

};//class RenderFramework

VK_NAMESPACE_END
