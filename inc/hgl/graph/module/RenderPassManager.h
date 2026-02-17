#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{

GRAPH_MODULE_CLASS(RenderPassManager)
{
    UnorderedMap<AnsiString,RenderPass *> RenderPassList;

private:

    RenderPassManager(GraphicsContext *);
    ~RenderPassManager();

    friend class GraphModuleManager;

private:

    RenderPass *    CreateRenderPass(   const AnsiString &name,
                                        const ValueArray<VkAttachmentDescription> &desc_list,
                                        const ValueArray<VkSubpassDescription> &subpass,
                                        const ValueArray<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

public:

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);

    void Release() override
    {
        if (RenderPassList.GetCount() > 0)
        {
            for (auto &kv : RenderPassList)
            {
                delete kv.second;
            }

            RenderPassList.Clear();
        }
    }
};//class RenderPassManager

}//namespace hgl::graph
