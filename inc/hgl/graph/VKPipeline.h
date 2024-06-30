#ifndef HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKPipelineData.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/io/DataOutputStream.h>
VK_NAMESPACE_BEGIN
class Pipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;

    const VIL *vil;
    PipelineData *data;

    bool alpha_test;
    bool alpha_blend;
    
private:

    friend class RenderPass;

    Pipeline(const AnsiString &n,VkDevice dev,VkPipeline p,const VIL *v,PipelineData *pd)
    {
        name=n;

        device=dev;
        pipeline=p;
        vil=v;
        data=pd;

        alpha_test=false;
        alpha_blend=false;
    }

public:

    virtual ~Pipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}

    const VIL *GetVIL()const{return vil;}
    const PipelineData *GetData()const{return data;}

    const bool IsAlphaTest()const{return data->alpha_test>0;}
    const bool IsAlphaBlend()const{return data->alpha_blend;}
};//class GraphicsPipeline
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_INCLUDE
 