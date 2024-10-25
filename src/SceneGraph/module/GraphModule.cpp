#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

GPUDeviceAttribute *GraphModule::GetDeviceAttribute()
{
    return module_manager->GetDevice()->GetDeviceAttribute();
}

VK_NAMESPACE_END
