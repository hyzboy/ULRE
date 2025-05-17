#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

        GPUDevice *         GraphModule::GetDevice          ()      {return render_framework->GetDevice();}
        VkDevice            GraphModule::GetVkDevice        ()const {return render_framework->GetVkDevice();}
const   GPUPhysicalDevice * GraphModule::GetPhysicalDevice  ()const {return render_framework->GetPhysicalDevice();}
        VkDevAttr *GraphModule::GetDeviceAttribute ()const {return render_framework->GetDeviceAttribute();}

        VkPipelineCache     GraphModule::GetPipelineCache   ()const {return render_framework->GetDeviceAttribute()->pipeline_cache;}

VK_NAMESPACE_END
