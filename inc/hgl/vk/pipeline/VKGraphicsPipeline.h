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

    VkFormat debug_depth_attachment_format = VK_FORMAT_UNDEFINED;
    uint32_t debug_color_attachment_count = 0;
    bool debug_created_with_gpl = false;

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

    void SetDebugRenderingSignature(const VkFormat depth_format,
                                   const uint32_t color_count,
                                   const bool created_with_gpl)
    {
        debug_depth_attachment_format = depth_format;
        debug_color_attachment_count = color_count;
        debug_created_with_gpl = created_with_gpl;
    }

    const VkFormat GetDebugDepthAttachmentFormat()const{return debug_depth_attachment_format;}
    const uint32_t GetDebugColorAttachmentCount()const{return debug_color_attachment_count;}
    const bool IsDebugCreatedWithGpl()const{return debug_created_with_gpl;}

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
