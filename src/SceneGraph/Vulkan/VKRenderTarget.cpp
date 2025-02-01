#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKBuffer.h>

VK_NAMESPACE_BEGIN

GPUDevice *IRenderTarget::GetDevice  ()const{return render_framework->GetDevice();}
VkDevice   IRenderTarget::GetVkDevice()const{return render_framework->GetDevice()->GetDevice();}

IRenderTarget::IRenderTarget(RenderFramework *rf,const VkExtent2D &ext)
{
    render_framework=rf;

    ubo_vp_info=GetDevice()->CreateUBO(sizeof(ViewportInfo),&vp_info);

    OnResize(ext);
}

IRenderTarget::~IRenderTarget()
{
    SAFE_CLEAR(ubo_vp_info);
}

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;
    vp_info.Set(ext.width,ext.height);
    ubo_vp_info->Write(&vp_info);
}

void RenderTargetData::Clear()
{
    SAFE_CLEAR(queue);
    SAFE_CLEAR(render_complete_semaphore);
    SAFE_CLEAR(fbo);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(color_textures,color_count);
    SAFE_CLEAR(depth_texture);
}

VK_NAMESPACE_END