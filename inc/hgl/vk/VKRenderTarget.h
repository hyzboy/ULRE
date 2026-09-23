#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/color/Color4f.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKCommandBuffer.h>
//#include<iostream>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

class VulkanDevice; // Forward declaration

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
protected:

    hgl::ecs::ECSContext *ecs_context;

    VkExtent2D extent;

    // 环境选择：本 RT 使用哪个环境 Profile（数据归 EnvironmentManager，
    // RT 只持引用；未设置即 default）。绑定由 RDBS 每帧按此解析。
    EnvProfileID env_profile = kEnvProfileDefault;

    // 清屏色：标准化后以 RT 上这份为权威（原散落在 WorkObject / ECSContext /
    // RenderSystemCore 三处；阶段 C 收敛为统一从 RT 读取）
    Color4f clear_color{0,0,0,1};

public:

    void SetEnvironmentProfile(EnvProfileID id) { env_profile = id; }
    EnvProfileID GetEnvironmentProfile() const { return env_profile; }

    void SetClearColor(const Color4f &color) { clear_color = color; }
    const Color4f &GetClearColor() const { return clear_color; }

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

    /// 是否为窗口交换链目标。
    ///
    /// 决定渲染结束时的附件最终布局：交换链颜色附件转 PRESENT_SRC_KHR 交呈现引擎，
    /// 离屏附件转可采样布局。
    ///
    /// @note 与 `RenderbufferInfo::IsSwapchain()` 命名一致。
    /// @note 本函数刻意声明在接口最末：新增虚函数若插在中间，会整体平移后续槽位，
    ///       任何未随头文件重编译的旧目标文件都会因 vtable 错位而调用到错误的函数
    ///       （表现为随机段错误）。放在末尾可让既有槽位偏移保持不变。
    virtual bool                IsSwapchain        ()const{return false;}

public:
    virtual ViewportInfo *      GetViewportInfo     ();
};//class IRenderTarget

}//namespace hgl::graph
