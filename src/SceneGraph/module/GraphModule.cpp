#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderFramework.h>
#include<hgl/vk/VKDevice.h>

VK_NAMESPACE_BEGIN

        VulkanDevice *      GraphModule::GetDevice          ()      {return graphics_context?graphics_context->GetDevice():render_framework->GetDevice();}
        VkDevice            GraphModule::GetVkDevice        ()const {return graphics_context?graphics_context->GetVkDevice():render_framework->GetVkDevice();}
const   VulkanPhyDevice *   GraphModule::GetPhyDevice       ()const {return graphics_context?graphics_context->GetPhyDevice():render_framework->GetPhyDevice();}
        VulkanDevAttr *     GraphModule::GetDevAttr         ()const {return graphics_context?graphics_context->GetDevAttr():render_framework->GetDevAttr();}
        VulkanSurface *     GraphModule::GetSurface         ()const {return render_framework->GetSurface();}

        VkPipelineCache     GraphModule::GetPipelineCache   ()const
        {
            auto *attr=graphics_context?graphics_context->GetDevAttr():render_framework->GetDevAttr();
            return attr?attr->pipeline_cache:VK_NULL_HANDLE;
        }

VK_NAMESPACE_END
