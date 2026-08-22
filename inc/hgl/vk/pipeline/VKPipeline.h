#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/io/DataOutputStream.h>

namespace hgl::graph{
class Pipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;

    PipelineData *data;
    bool overlay;

    bool alpha_test;
    bool alpha_blend;

private:

    friend class RenderPass;

    Pipeline(const AnsiString &n,
             VkDevice dev,
             VkPipeline p,
             PipelineData *pd,
             const bool is_overlay)
    {
        name=n;
        device=dev;
        pipeline=p;
        data=pd;
        overlay=is_overlay;

        alpha_test=false;
        alpha_blend=false;
    }

public:

    virtual ~Pipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}
    const PipelineData *GetData()const{return data;}
    const bool GetOverlay()const{return overlay;}

    const bool IsAlphaTest()const{return data->alpha_test>0;}
    const bool IsAlphaBlend()const{return data->alpha_blend;}
};//class GraphicsPipeline
}//namespace hgl::graph
