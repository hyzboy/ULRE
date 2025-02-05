#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Map.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN

using RenderPassHASHCode=util::HashCode<128/8>;

inline util::Hash *CreateRenderPassHash()
{
    return util::CreateHash(util::HASH::xxH3_128);
}

GRAPH_MODULE_CLASS(RenderPassManager)
{
    util::Hash *hash;

    Map<RenderPassHASHCode,RenderPass *> RenderPassList;

private:

    RenderPassManager(RenderFramework *);
    ~RenderPassManager();

    friend class GraphModuleManager;

private:
    
    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

public:

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);
};//class RenderPassManager

VK_NAMESPACE_END
