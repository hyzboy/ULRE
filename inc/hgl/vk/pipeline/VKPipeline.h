#pragma once

#include<hgl/vk/VK.h>
#include<hgl/mtl/PipelineConfig.h>
#include<hgl/io/DataOutputStream.h>

namespace hgl::graph{
class Pipeline
{
    VkDevice device;

    AnsiString name;

    VkPipeline pipeline;

    mtl::MaterialPipelineConfig config;
    bool overlay;

private:

    friend class RenderPass;

    Pipeline(const AnsiString &n,
             VkDevice dev,
             VkPipeline p,
             const mtl::MaterialPipelineConfig &cfg,
             const bool is_overlay)
    {
        name=n;
        device=dev;
        pipeline=p;
        config=cfg;
        overlay=is_overlay;
    }

public:

    virtual ~Pipeline();

    const AnsiString &GetName()const{return name;}

    operator VkPipeline(){return pipeline;}
    const mtl::MaterialPipelineConfig &GetConfig()const{return config;}
    const bool GetOverlay()const{return overlay;}
};//class Pipeline
}//namespace hgl::graph
