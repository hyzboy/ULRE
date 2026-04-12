#include <hgl/graph/module/SamplerManager.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKObjectNameBuilder.h>
#include <cstdint>

namespace hgl::graph{

GRAPH_MODULE_CONSTRUCT(SamplerManager)
{
}

std::weak_ptr<Sampler> SamplerManager::CreateSampler(VkSamplerCreateInfo *sci)
{
    auto dev = GetDevice();
    Sampler *sampler = dev->CreateSampler(sci);
    if (!sampler)
        return {};

    AnsiString name = "Sampler_" + AnsiString::numberOf((uint64_t)(uintptr_t)sampler);
    VkSampler vk_sampler = *sampler;
    dev->TrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)vk_sampler, ObjectNameBuilder(name.c_str()));
    SamplerID id = Add(sampler);
    return rm_samplers.Get(id);
}

std::weak_ptr<Sampler> SamplerManager::CreateSampler(Texture *tex)
{
    auto dev = GetDevice();
    Sampler *sampler = dev->CreateSampler(tex);
    if (!sampler)
        return {};

    AnsiString name = "Sampler_" + AnsiString::numberOf((uint64_t)(uintptr_t)sampler);
    VkSampler vk_sampler = *sampler;
    dev->TrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)vk_sampler, ObjectNameBuilder(name.c_str()));
    SamplerID id = Add(sampler);
    return rm_samplers.Get(id);
}

}//namespace hgl::graph
