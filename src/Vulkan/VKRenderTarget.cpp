#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKBuffer.h>

#include<hgl/graph/mtl/UBOCommon.h>     //未来UBO统合看能不能不引用

VK_NAMESPACE_BEGIN

VulkanDevice *IRenderTarget::GetDevice  ()const
{
    if(render_framework)
        return render_framework->GetDevice();

    if(ecs_context)
        return ecs_context->GetGPUDevice();

    return nullptr;
}

VkDevice IRenderTarget::GetVkDevice()const
{
    auto *device = GetDevice();
    return device ? device->GetDevice() : nullptr;
}

IRenderTarget::IRenderTarget(RenderFramework *rf,const VkExtent2D &ext):desc_binding(DescriptorSetType::RenderTarget)
{
    render_framework=rf;
    ecs_context=nullptr;

    VulkanDevice *device=GetDevice();
    if(device)
    {
        ubo_vp_info=device->CreateUBO<UBOViewportInfo>(&mtl::SBS_ViewportInfo,BufferUpdateClass::CriticalPerFrame);
        desc_binding.AddUBO(ubo_vp_info);
        OnResize(ext);
    }
    else
    {
        ubo_vp_info=nullptr;
        extent=ext;
    }
}

IRenderTarget::IRenderTarget(hgl::ecs::ECSContext *ctx,const VkExtent2D &ext):desc_binding(DescriptorSetType::RenderTarget)
{
    render_framework=nullptr;
    ecs_context=ctx;

    VulkanDevice *device=GetDevice();
    if(device)
    {
        ubo_vp_info=device->CreateUBO<UBOViewportInfo>(&mtl::SBS_ViewportInfo,BufferUpdateClass::CriticalPerFrame);
        desc_binding.AddUBO(ubo_vp_info);
        OnResize(ext);
    }
    else
    {
        ubo_vp_info=nullptr;
        extent=ext;
    }
}

IRenderTarget::~IRenderTarget()
{
    SAFE_CLEAR(ubo_vp_info);
}

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;

    if(!ubo_vp_info)
        return;

    ubo_vp_info->Data()->Set(ext.width,ext.height);

    ubo_vp_info->ImmediateUpdate();  // 立即同步到 GPU / Immediate sync to GPU
}

VK_NAMESPACE_END
