#pragma once

#include<hgl/graph/VK.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/module/GraphModuleManager.h>

VK_NAMESPACE_BEGIN

class RenderPassManager;

class RenderFramework:public io::WindowEvent
{
    OSString            app_name;

    Window *            win                 =nullptr;
    VulkanInstance *    inst                =nullptr;

    GPUDevice *         device              =nullptr;

protected:

    GraphModuleManager *module_manager      =nullptr;

    RenderPassManager * render_pass_manager =nullptr;
    RenderPass *        device_render_pass  =nullptr;

public:

    Window *            GetWindow       (){return win;}
    GPUDevice *         GetDevice       (){return device;}

    RenderPass *        GetRenderPass   (){return device_render_pass;}

public:

    GraphModuleManager *GetModuleManager(){return module_manager;}
    RenderPassManager * GetRenderPassManager(){return render_pass_manager;}


public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public: // event

    void OnResize(uint w,uint h);
    void OnActive(bool);
    void OnClose();

};//class RenderFramework

VK_NAMESPACE_END
