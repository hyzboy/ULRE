#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Map.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN
using RenderPassHASHCode=util::HashCodeSHA1LE;

class DeviceRenderPassManage:public GraphModule
{
    VkPipelineCache pipeline_cache;

    util::Hash *hash;

    Map<RenderPassHASHCode,RenderPass *> RenderPassList;

private:

    friend class GPUDevice;

    //DeviceRenderPassManage(VkDevice,VkPipelineCache);
    GRAPH_MODULE_CONSTRUCT(DeviceRenderPassManage)
    ~DeviceRenderPassManage();

    bool Init() override;

private:
    
    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *,const uint subpass_count=2);
};//class DeviceRenderPassManage
VK_NAMESPACE_END
