#pragma once

#include <hgl/graph/module/GraphModule.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/type/ObjectManager.h>

namespace hgl::graph{

using SamplerID = int;

GRAPH_MODULE_CLASS(SamplerManager)
{
private:
    SharedObjectManager<SamplerID, Sampler> rm_samplers; ///<采样器合集

    SamplerManager(GraphicsContext *);
    ~SamplerManager() = default;

    friend class GraphModuleManager;

public:
    SamplerID                Add    (Sampler *s)              { return rm_samplers.Add(s); }
    std::weak_ptr<Sampler>   Get    (const SamplerID &id)     { return rm_samplers.Get(id); }
    std::shared_ptr<Sampler> Acquire(const SamplerID &id)     { return rm_samplers.Acquire(id); }
    void                     Release(Sampler *s)               { rm_samplers.Release(s); }
    void                     Release(const SamplerID &id)      { rm_samplers.Release(id); }

    void Release() override
    {
        if (rm_samplers.GetCount() > 0)
            rm_samplers.Clear();
    }

    /// 创建并由 Manager 托管，返回 weak_ptr（Manager 持有所有权）
    std::weak_ptr<Sampler> CreateSampler(VkSamplerCreateInfo *sci = nullptr);
    std::weak_ptr<Sampler> CreateSampler(Texture *tex);
};

}//namespace hgl::graph
