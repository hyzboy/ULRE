#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKBuffer.h>

#include<hgl/graph/mtl/UBOCommon.h>     //未来UBO统合看能不能不引用

VK_NAMESPACE_BEGIN

GPUDevice *IRenderTarget::GetDevice  ()const{return render_framework->GetDevice();}
VkDevice   IRenderTarget::GetVkDevice()const{return render_framework->GetDevice()->GetDevice();}

IRenderTarget::IRenderTarget(RenderFramework *rf,const VkExtent2D &ext):desc_binding(DescriptorSetType::RenderTarget)
{
    render_framework=rf;

    ubo_vp_info=GetDevice()->CreateUBO<UBOViewportInfo>();

    desc_binding.AddUBO(mtl::SBS_ViewportInfo.name,*ubo_vp_info);

    OnResize(ext);
}

IRenderTarget::~IRenderTarget()
{
    SAFE_CLEAR(ubo_vp_info);
}

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;

    ubo_vp_info->data()->Set(ext.width,ext.height);

    ubo_vp_info->Update();
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
