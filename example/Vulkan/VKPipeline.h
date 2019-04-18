#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN
class Pipeline
{
    VkPipeline pipeline;

public:

    Pipeline(VkPipeline p){pipeline=p;}
    virtual ~Pipeline();
};//class GraphicsPipeline

class Shader;
class VertexInput;

class PipelineCreater
{
    VkGraphicsPipelineCreateInfo pipelineInfo;

    VkPipelineVertexInputStateCreateInfo vis_create_info;

private:

    const Shader *        shader      =nullptr;
    const VertexInput *   vertex_input=nullptr;

public:

    PipelineCreater()
    {
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    }
    ~PipelineCreater();

    bool Set(const Shader *);
    bool Set(const VertexInput *);

    Pipeline *Create();
};//class PipelineCreater
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
