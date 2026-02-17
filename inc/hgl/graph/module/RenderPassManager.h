#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/UnorderedMap.h>
#include<iostream>

namespace hgl::graph{

GRAPH_MODULE_CLASS(RenderPassManager)
{
    UnorderedMap<AnsiString,RenderPass *> RenderPassList;

private:

    RenderPassManager(GraphicsContext *);
    ~RenderPassManager()
    {
        Release();
    }

    friend class GraphModuleManager;

private:

    RenderPass *    CreateRenderPass(   const AnsiString &name,
                                        const ValueArray<VkAttachmentDescription> &desc_list,
                                        const ValueArray<VkSubpassDescription> &subpass,
                                        const ValueArray<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

public:

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);

    void Release() override;
};//class RenderPassManager

}//namespace hgl::graph
