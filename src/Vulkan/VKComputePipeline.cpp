#include<hgl/vk/pipeline/VKComputePipeline.h>

namespace hgl::graph{

ComputePipeline::~ComputePipeline()
{
    if(pipeline)
        vkDestroyPipeline(device, pipeline, nullptr);

    // 专用 layout（CreateComputePipeline owns_pipeline_layout=true）随管线销毁；
    // 共享全局 layout 由 ShaderProgramManager 单例持有，这里不动
    if(owns_pipeline_layout && pipeline_layout!=VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
}

}//namespace hgl::graph
