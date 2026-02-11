#include<hgl/graph/VKComputePipeline.h>

VK_NAMESPACE_BEGIN

ComputePipeline::~ComputePipeline()
{
    if(pipeline)
        vkDestroyPipeline(device, pipeline, nullptr);
}

VK_NAMESPACE_END
