#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKSwapchain.h>
#include<hgl/graph/VKQueue.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
//#include<iostream>

VK_NAMESPACE_BEGIN

class RenderFramework;

using UBOViewportInfo=UBOInstance<graph::ViewportInfo>;

class IRenderTarget
{
    RenderFramework *render_framework;

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

public:

    virtual ViewportInfo *      GetViewportInfo     ()
    {
        return ubo_vp_info->data();
    }
};//class IRenderTarget

VK_NAMESPACE_END
