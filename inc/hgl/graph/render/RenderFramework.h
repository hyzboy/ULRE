#pragma once

#include<hgl/platform/Window.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/ecs/core/Context.h>
#include <memory>

VK_NAMESPACE_BEGIN

namespace mtl
{
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;
}//namespace mtl

class FontDataSource;
class TileFont;
class FontSource;
class RenderPassManager;
class TextureManager;
class RenderTargetManager;
class MaterialManager;
class BufferManager;

class RenderModule;

class LineRenderManager; // forward
class RenderContext;     // forward

class RenderFramework:public io::WindowEvent
{
    OSString                app_name;

    Window *                win                 =nullptr;
    VulkanInstance *        inst                =nullptr;

    VulkanDevice *          device              =nullptr;

protected:

    GraphModuleManager *    module_manager      =nullptr;

    RenderPassManager *     rp_manager          =nullptr;

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;
    MaterialManager *       material_manager    =nullptr;
    BufferManager *         buffer_manager      =nullptr;
    SamplerManager *        sampler_manager     =nullptr;
    GeometryManager *       geometry_manager    =nullptr;
    PrimitiveManager *      primitive_manager   =nullptr;

    SwapchainModule *       sc_module           =nullptr;

protected:

    ecs::ECSContext *       default_ecs_context     =nullptr;
    std::unique_ptr<graph::RenderContext> render_context;

protected:  //EventDispatcher

    Vector2i mouse_coord;

    virtual io::EventProcResult OnEvent(const io::EventHeader &header,const uint64 data) override;

public:

            Window *            GetWindow           ()const{return win;}
            VulkanDevice *      GetDevice           ()const{return device;}
            VkDevice            GetVkDevice         ()const{return device->GetDevice();}
    const   VulkanPhyDevice *   GetPhyDevice        ()const{return device->GetPhyDevice();}
            VulkanDevAttr *     GetDevAttr          ()const{return device->GetDevAttr();}
            VulkanSurface *     GetSurface          ()const{return device->GetDevAttr()->surface;}

public:

    GraphModuleManager *    GetModuleManager        (){return module_manager;}

    RenderPassManager *     GetRenderPassManager    (){return rp_manager;}
    TextureManager *        GetTextureManager       (){return tex_manager;}
    RenderTargetManager *   GetRenderTargetManager  (){return rt_manager;}
    MaterialManager *       GetMaterialManager      (){return material_manager;}
    BufferManager *         GetBufferManager        (){return buffer_manager;}
    SamplerManager *        GetSamplerManager       (){return sampler_manager;}
    GeometryManager *       GetGeometryManager      (){return geometry_manager;}
    PrimitiveManager *      GetPrimitiveManager     (){return primitive_manager;}

    SwapchainModule *       GetSwapchainModule      (){return sc_module;}
    SwapchainRenderTarget * GetSwapchainRenderTarget(){return sc_module?sc_module->GetRenderTarget():nullptr;}
    SwapchainRenderTarget * GetSwapchainRenderTarget() const {return sc_module?sc_module->GetRenderTarget():nullptr;}

public:

    ecs::ECSContext *       GetECSContext           (){return default_ecs_context;}
    graph::RenderContext *  GetRenderContext        (){return render_context.get();}
    RenderPass *            GetDefaultRenderPass    ()const
    {
        auto *rt = GetSwapchainRenderTarget();
        return rt ? rt->GetRenderPass() : nullptr;
    }

    LineRenderManager *     GetLineRenderManager    ()const;

public:

    const Vector2i &GetMouseCoord()const{ return mouse_coord; }

public:

    RenderFramework(const OSString &);
    virtual ~RenderFramework();

    virtual bool Init(uint w,uint h);

public: // event

    virtual void OnResize(uint w,uint h);
    virtual void OnActive(bool);
    virtual void OnClose();

public:

    void Tick();

public: // other
};//class RenderFramework

VK_NAMESPACE_END

