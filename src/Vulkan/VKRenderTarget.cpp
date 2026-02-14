#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/vk/VKBuffer.h>

#include<hgl/graph/mtl/UBOCommon.h>     //未来UBO统合看能不能不引用

VK_NAMESPACE_BEGIN

VulkanDevice *IRenderTarget::GetDevice  ()const{return render_framework->GetDevice();}
VkDevice   IRenderTarget::GetVkDevice()const{return render_framework->GetDevice()->GetDevice();}

IRenderTarget::IRenderTarget(RenderFramework *rf,const VkExtent2D &ext):desc_binding(DescriptorSetType::RenderTarget)
{
    render_framework=rf;

    ubo_vp_info=GetDevice()->CreateUBO<UBOViewportInfo>(&mtl::SBS_ViewportInfo,BufferUpdateClass::CriticalPerFrame);

    desc_binding.AddUBO(ubo_vp_info);

    OnResize(ext);
}

IRenderTarget::~IRenderTarget()
{
    SAFE_CLEAR(ubo_vp_info);
}

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;

    ubo_vp_info->Data()->Set(ext.width,ext.height);

    ubo_vp_info->ImmediateUpdate();  // 立即同步到 GPU / Immediate sync to GPU
}

VK_NAMESPACE_END
