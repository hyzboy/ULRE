#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKPipelineData.h>
#include<hgl/io/DataOutputStream.h>
VK_NAMESPACE_BEGIN
class Pipeline
{
    VkDevice device;
    VkPipeline pipeline;
    PipelineData *data;

    bool alpha_test;
    bool alpha_blend;

public:

    Pipeline(VkDevice dev,VkPipeline p,PipelineData *pd):device(dev),pipeline(p),data(pd){}
    virtual ~Pipeline();

    operator VkPipeline(){return pipeline;}

    const PipelineData *GetData()const{return data;}

    const bool IsAlphaTest()const{return data->alpha_test>0;}
    const bool IsAlphaBlend()const{return data->alpha_blend;}
};//class GraphicsPipeline

Pipeline *CreatePipeline(Device *dev,PipelineData *,const Material *material,const RenderTarget *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
