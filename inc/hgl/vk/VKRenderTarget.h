#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDescriptorBindingManage.h>
//#include<iostream>

namespace hgl::ecs
{
    class ECSContext;
}

VK_NAMESPACE_BEGIN

class RenderFramework;

using UBOViewportInfo=StructuredBufferAccessor<ViewportInfo>;  ///< 统一使用 StructuredBufferAccessor

class IRenderTarget
{
    RenderFramework *render_framework;
    hgl::ecs::ECSContext *ecs_context;

    VkExtent2D extent;

    UBOViewportInfo *ubo_vp_info;

    DescriptorBinding desc_binding;

public:

    RenderFramework *   GetRenderFramework  ()const{return render_framework;}
    VulkanDevice *      GetDevice           ()const;
    VkDevice            GetVkDevice         ()const;
    DescriptorBinding * GetDescriptorBinding(){return &desc_binding;}

    const   VkExtent2D &GetExtent       ()const{return extent;}

    virtual uint32_t    GetColorCount   ()=0;
    virtual bool        hasDepth        ()=0;

public:

    void OnResize(const VkExtent2D &ext);

public:

    IRenderTarget(RenderFramework *,const VkExtent2D &);
    IRenderTarget(hgl::ecs::ECSContext *,const VkExtent2D &);
    virtual ~IRenderTarget();

    virtual Framebuffer *       GetFramebuffer  ()=0;
    virtual RenderPass *        GetRenderPass   ()=0;

    virtual Texture2D *         GetColorTexture (const int index=0)=0;
    virtual Texture2D *         GetDepthTexture ()=0;

public: // Command Buffer

    virtual DeviceQueue *       GetQueue            ()=0;
    virtual Semaphore *         GetRenderCompleteSemaphore()=0;

    virtual RenderCmdBuffer *   GetRenderCmdBuffer  ()=0;

    virtual bool                Submit              (Semaphore *wait_sem)=0;

    virtual bool                Submit              (){return Submit(nullptr);}

    virtual bool                WaitQueue           ()=0;
    virtual bool                WaitFence           ()=0;

    virtual RenderCmdBuffer *   BeginRender         ()=0;
    virtual void                EndRender           ()=0;

    virtual uint32_t            GetCurrentFrameIndex()const{return 0;}
    virtual uint32_t            GetFrameCount       ()const{return 1;}

public:
    virtual ViewportInfo *      GetViewportInfo     ();
};//class IRenderTarget

VK_NAMESPACE_END
