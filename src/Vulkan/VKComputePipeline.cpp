#include<hgl/vk/pipeline/VKComputePipeline.h>

namespace hgl::graph{

ComputePipeline::~ComputePipeline()
{
    if(pipeline)
        vkDestroyPipeline(device, pipeline, nullptr);
}

}//namespace hgl::graph
