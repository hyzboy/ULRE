#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{

        VulkanDevice *      GraphModule::GetDevice          ()      {return graphics_context?graphics_context->GetDevice():nullptr;}
        VkDevice            GraphModule::GetVkDevice        ()const {return graphics_context?graphics_context->GetVkDevice():VK_NULL_HANDLE;}
const   VulkanPhyDevice *   GraphModule::GetPhyDevice       ()const {return graphics_context?graphics_context->GetPhyDevice():nullptr;}
const   mtl::contract::PhysicalDeviceProfileLite *GraphModule::GetPhysicalDeviceProfile()const
{
        return graphics_context?graphics_context->GetPhysicalDeviceProfile():nullptr;
}
        VulkanDevAttr *     GraphModule::GetDevAttr         ()const {return graphics_context?graphics_context->GetDevAttr():nullptr;}
        VulkanSurface *     GraphModule::GetSurface         ()const
        {
            auto *attr = graphics_context?graphics_context->GetDevAttr():nullptr;
            return attr?attr->surface:nullptr;
        }

                VkPipelineCache     GraphModule::GetPipelineCache   ()const
                {
                        auto *attr=graphics_context?graphics_context->GetDevAttr():nullptr;
                        return attr?attr->pipeline_cache:VK_NULL_HANDLE;
                }

}//namespace hgl::graph
