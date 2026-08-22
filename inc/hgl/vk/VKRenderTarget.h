#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKCommandBuffer.h>
//#include<iostream>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

class VulkanDevice; // Forward declaration

using UBOViewportInfo=StructuredBufferAccessor<ViewportInfo>;  ///< 统一使用 StructuredBufferAccessor

// Dynamic Rendering 附件描述：image view + format + 布局
// （替代传统 render pass 的 attachment 声明——渲染循环直接据此构造 VkRenderingInfoKHR）
struct RenderingAttachment
{
    VkImageView     image_view = VK_NULL_HANDLE;
    VkFormat        format = VK_FORMAT_UNDEFINED;
    VkImageLayout   initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout   final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const bool IsValid()const{return image_view!=VK_NULL_HANDLE;}
};

class IRenderTarget
{
    hgl::ecs::ECSContext *ecs_context;

    VkExtent2D extent;

public:

    VulkanDevice *      GetDevice           ()const;
    VkDevice            GetVkDevice         ()const;

    const   VkExtent2D &GetExtent       ()const{return extent;}

    virtual uint32_t    GetColorCount   ()=0;
    virtual bool        hasDepth        ()=0;

public:

    void OnResize(const VkExtent2D &ext);

public:

    IRenderTarget(hgl::ecs::ECSContext *,const VkExtent2D &);
    virtual ~IRenderTarget();

    virtual Framebuffer *       GetFramebuffer  ()=0;
    virtual RenderPass *        GetRenderPass   ()=0;

    virtual Texture2D *         GetColorTexture (const int index=0)=0;
    virtual Texture2D *         GetDepthTexture ()=0;

    // Dynamic Rendering 附件（index 越界返回 invalid）
    virtual RenderingAttachment GetColorAttachment(const int index=0)
    {
        RenderingAttachment att;
        Texture2D *tex=GetColorTexture(index);
        if(!tex)
            return att;
        att.image_view=tex->GetVulkanImageView();
        att.format=tex->GetFormat();
        return att;
    }
    virtual RenderingAttachment GetDepthAttachment()
    {
        RenderingAttachment att;
        Texture2D *tex=GetDepthTexture();
        if(!tex)
            return att;
        att.image_view=tex->GetVulkanImageView();
        att.format=tex->GetFormat();
        att.final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        return att;
    }

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

}//namespace hgl::graph
