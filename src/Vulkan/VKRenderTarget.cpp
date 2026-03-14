#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/StructuredBufferAccessor.h>

#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>

#include<hgl/mtl/UBOCommon.h>     //未来UBO统合看能不能不引用

namespace hgl::graph{

namespace
{
    hgl::graph::UBOViewportInfo *CreateViewportUBO(hgl::ecs::ECSContext *ctx)
    {
        if (!ctx)
            return nullptr;

        auto *gc = ctx->GetGraphicsContext();
        if (!gc)
        {
            if (auto *rc = ctx->GetRenderContext())
                gc = rc->GetGraphicsContext();
        }

        if (gc)
        {
            auto *buffer_manager = gc->GetBufferManager();
            if (!buffer_manager)
                return nullptr;

            auto *buf = buffer_manager->CreateUBO("ViewportInfoUBO", hgl::graph::StructuredBufferAccessor<hgl::graph::ViewportInfo>::GetSize());
            if (!buf)
                return nullptr;

            buf->SetUpdateClass(hgl::graph::BufferUpdateClass::CriticalPerFrame);
            return hgl::graph::StructuredBufferAccessor<hgl::graph::ViewportInfo>::Create(buf, &hgl::graph::mtl::SBS_ViewportInfo, false);
        }

        return nullptr;
    }

    hgl::graph::BufferManager *GetBufferManager(hgl::ecs::ECSContext *ctx)
    {
        if (!ctx)
            return nullptr;

        if (auto *rc = ctx->GetRenderContext())
        {
            if (auto *gc = rc->GetGraphicsContext())
                return gc->GetBufferManager();
        }

        if (auto *gc = ctx->GetGraphicsContext())
            return gc->GetBufferManager();

        return nullptr;
    }
}

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
    if(!ubo_vp_info)
    {
        ubo_vp_info = CreateViewportUBO(ecs_context);
        ubo_vp_info_managed = (ubo_vp_info != nullptr);

        if(ubo_vp_info)
        {
            desc_binding.AddUBO(ubo_vp_info);
            ubo_vp_info->Data()->Set(extent.width, extent.height);
            ubo_vp_info->MarkDirty();
        }
    }

    return ubo_vp_info ? ubo_vp_info->Data() : nullptr;
}

IRenderTarget::IRenderTarget(hgl::ecs::ECSContext *ctx,const VkExtent2D &ext):desc_binding(DescriptorSetType::Scene)
{
    ecs_context=ctx;

    ubo_vp_info = CreateViewportUBO(ecs_context);
    ubo_vp_info_managed = (ubo_vp_info != nullptr);

    if(ubo_vp_info)
    {
        desc_binding.AddUBO(ubo_vp_info);
        OnResize(ext);
    }
    else
    {
        extent=ext;
    }
}

IRenderTarget::~IRenderTarget()
{
    if (ubo_vp_info)
    {
        VkBufferOwner *buf = ubo_vp_info->ubo();
        delete ubo_vp_info;
        ubo_vp_info = nullptr;

        if (ubo_vp_info_managed && buf)
        {
            if (auto *buffer_manager = GetBufferManager(ecs_context))
                buffer_manager->Release(buf);
        }
    }
    ubo_vp_info_managed = false;
}

void IRenderTarget::OnResize(const VkExtent2D &ext)
{
    extent=ext;

    if(!ubo_vp_info)
    {
        GetViewportInfo();
        return;
    }

    ubo_vp_info->Data()->Set(ext.width,ext.height);

    ubo_vp_info->MarkDirty();
}

}//namespace hgl::graph
