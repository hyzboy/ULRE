#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/type/ObjectManager.h>

VK_NAMESPACE_BEGIN

using SamplerID = int;

GRAPH_MODULE_CLASS(SamplerManager)
{
private:
    AutoIdObjectManager<SamplerID, Sampler> rm_samplers; ///<采样器合集

    SamplerManager(IGraphicsContext *);
    ~SamplerManager() = default;

    friend class GraphModuleManager;

public:
    SamplerID Add(Sampler *s) { return rm_samplers.Add(s); }
    Sampler *Get(const SamplerID &id) { return rm_samplers.Get(id); }
    void Release(Sampler *s) { rm_samplers.Release(s); }

    void Release() override
    {
        if (rm_samplers.GetCount() > 0)
            rm_samplers.Clear();
    }

    Sampler *CreateSampler(VkSamplerCreateInfo *sci = nullptr);
    Sampler *CreateSampler(Texture *tex);
};

VK_NAMESPACE_END
