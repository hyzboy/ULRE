#include<hgl/graph/pipeline/VKPipeline.h>

VK_NAMESPACE_BEGIN
Pipeline::~Pipeline()
{
    delete data;
    vkDestroyPipeline(device,pipeline,nullptr);
}
VK_NAMESPACE_END
