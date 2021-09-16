#include<hgl/graph/VKPipeline.h>

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    vkDestroyPipeline(device,pipeline,nullptr);
}
VK_NAMESPACE_END
