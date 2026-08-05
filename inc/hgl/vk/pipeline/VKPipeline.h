#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/graph/PipelinePreset.h>
#include<hgl/io/DataOutputStream.h>

namespace hgl::graph{
class Pipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;

    const VIL *vil;
    PipelineData *data;
    PipelinePreset preset;

    bool alpha_test;
    bool alpha_blend;

private:

    friend class RenderPass;

    Pipeline(const AnsiString &n,
             VkDevice dev,
             VkPipeline p,
             const VIL *v,
             PipelineData *pd,
             PipelinePreset pp)
    {
        name=n;
        device=dev;
        pipeline=p;
        vil=v;
        data=pd;
        preset=pp;

        alpha_test=false;
        alpha_blend=false;
    }

public:

    virtual ~Pipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}

    const VIL *GetVIL()const{return vil;}
    const PipelineData *GetData()const{return data;}
    PipelinePreset GetPreset()const{return preset;}

    const bool IsAlphaTest()const{return data->alpha_test>0;}
    const bool IsAlphaBlend()const{return data->alpha_blend;}
};//class GraphicsPipeline
}//namespace hgl::graph
