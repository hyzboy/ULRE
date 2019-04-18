#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include"VK.h"

VK_NAMESPACE_BEGIN
class Pipeline
{
    VkDevice device;
    VkPipeline pipeline;

public:

    Pipeline(VkDevice dev,VkPipeline p){device=dev;pipeline=p;}
    virtual ~Pipeline();
};//class GraphicsPipeline

class Shader;
class VertexInput;

class PipelineCreater
{
    VkDevice device;
    uint width,height;
    VkGraphicsPipelineCreateInfo pipelineInfo;

    VkPipelineVertexInputStateCreateInfo vis_create_info;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;

    VkViewport viewport;
    VkRect2D scissor;
    VkPipelineViewportStateCreateInfo viewportState;

private:

    const Shader *        shader      =nullptr;
    const VertexInput *   vertex_input=nullptr;

public:

    PipelineCreater(VkDevice dev,uint w,uint h);
    ~PipelineCreater();

    bool Set(const Shader *);
    bool Set(const VertexInput *);
    bool Set(const VkPrimitiveTopology,bool=false);

    Pipeline *Create();
};//class PipelineCreater
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
