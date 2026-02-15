#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/UnorderedMap.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CLASS(RenderPassManager)
{
    UnorderedMap<AnsiString,RenderPass *> RenderPassList;

private:

    RenderPassManager(IGraphicsContext *);
    ~RenderPassManager();

    friend class GraphModuleManager;

private:

    RenderPass *    CreateRenderPass(   const ValueArray<VkAttachmentDescription> &desc_list,
                                        const ValueArray<VkSubpassDescription> &subpass,
                                        const ValueArray<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

public:

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);

    void Release() override
    {
        if (RenderPassList.GetCount() > 0)
            RenderPassList.Clear();
    }
};//class RenderPassManager

VK_NAMESPACE_END
