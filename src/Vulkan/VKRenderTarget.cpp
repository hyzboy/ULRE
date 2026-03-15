#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

namespace hgl::graph{

VulkanDevice *IRenderTarget::GetDevice  ()const
{
    if(ecs_context)
        return ecs_context->GetGPUDevice();

    return nullptr;
}

VkDevice IRenderTarget::GetVkDevice()const
{
    auto *device = GetDevice();
    return device ? device->GetDevice() : nullptr;
}

ViewportInfo *IRenderTarget::GetViewportInfo()
{
    if (!ecs_context)
        return nullptr;

    auto sys = ecs_context->GetSystem<hgl::ecs::RenderDescriptorBindingSystem>();
    return sys ? sys->GetViewportInfo() : nullptr;
}

IRenderTarget::IRenderTarget(hgl::ecs::ECSContext *ctx,const VkExtent2D &ext):desc_binding(DescriptorSetType::Scene)
{
    ecs_context=ctx;
    OnResize(ext);
}

IRenderTarget::~IRenderTarget() = default;

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;

    if (ecs_context)
    {
        auto sys = ecs_context->GetSystem<hgl::ecs::RenderDescriptorBindingSystem>();
        if (sys)
            sys->SetViewportExtent(ext.width, ext.height);
    }
}

}//namespace hgl::graph
