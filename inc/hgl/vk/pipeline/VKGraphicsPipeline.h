#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/io/DataOutputStream.h>

namespace hgl::graph{
class MonolithicGraphicsPipelineBuilder;
class GplGraphicsPipelineBuilder;
class GraphicsPipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;
    bool mesh_pipeline;

    const VIL *vil;
    GraphicsPipelineData *data;

private:

    friend class MonolithicGraphicsPipelineBuilder;
    friend class GplGraphicsPipelineBuilder;

    GraphicsPipeline(const AnsiString &n,VkDevice dev,VkPipeline p,bool mp,const VIL *v,GraphicsPipelineData *pd)
    {
        name=n;

        device=dev;
        pipeline=p;
        mesh_pipeline=mp;
        vil=v;
        data=pd;
    }

public:

    virtual ~GraphicsPipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}

    const VIL *GetVIL()const{return vil;}
    const GraphicsPipelineData *GetData()const{return data;}
    const bool IsMeshPipeline()const{return mesh_pipeline;}

    const bool IsAlphaTest()const{return data && data->alpha_test>0;}
    const bool IsAlphaBlend()const{return data && data->alpha_blend;}
};//class GraphicsPipeline

// Semantic aliases for the 4 Vulkan Graphics Pipeline Library stages.
// All resolve to GraphicsPipeline; the alias name conveys which stage's
// contract the caller is binding to.
using GraphicsPipelineVertexInput    = GraphicsPipeline;
using GraphicsPipelinePreRaster      = GraphicsPipeline;
using GraphicsPipelineFragment       = GraphicsPipeline;
using GraphicsPipelineFragmentOutput = GraphicsPipeline;

}//namespace hgl::graph
