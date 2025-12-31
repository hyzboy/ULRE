#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

        VulkanDevice *      GraphModule::GetDevice          ()      {return render_framework->GetDevice();}
        VkDevice            GraphModule::GetVkDevice        ()const {return render_framework->GetVkDevice();}
const   VulkanPhyDevice *   GraphModule::GetPhyDevice       ()const {return render_framework->GetPhyDevice();}
        VulkanDevAttr *     GraphModule::GetDevAttr         ()const {return render_framework->GetDevAttr();}
        VulkanSurface *     GraphModule::GetSurface         ()const {return render_framework->GetSurface();}

        VkPipelineCache     GraphModule::GetPipelineCache   ()const {return render_framework->GetDevAttr()->pipeline_cache;}

VK_NAMESPACE_END
