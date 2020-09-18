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

    bool alpha_test;
    bool alpha_blend;

public:

    Pipeline(VkDevice dev,VkPipeline p,bool at,bool ab):device(dev),pipeline(p),alpha_test(at),alpha_blend(ab){}
    virtual ~Pipeline();

    operator VkPipeline(){return pipeline;}

    const bool IsAlphaTest()const{return alpha_test;}
    const bool IsAlphaBlend()const{return alpha_blend;}
};//class GraphicsPipeline

Pipeline *CreatePipeline(Device *dev,VKPipelineData *,const Material *material,const RenderTarget *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
