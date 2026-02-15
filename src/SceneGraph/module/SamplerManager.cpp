#include <hgl/graph/module/SamplerManager.h>
#include <hgl/vk/VKDevice.h>
#include <cstdint>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CONSTRUCT(SamplerManager)
{
}

Sampler *SamplerManager::CreateSampler(VkSamplerCreateInfo *sci)
{
    auto dev = GetDevice();
    Sampler *sampler = dev->CreateSampler(sci);
    if (sampler)
    {
        AnsiString name = "Sampler_" + AnsiString::numberOf((uint64_t)(uintptr_t)sampler);
        VkSampler vk_sampler = *sampler;
        dev->TrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)vk_sampler, name);
        Add(sampler);
    }
    return sampler;
}

Sampler *SamplerManager::CreateSampler(Texture *tex)
{
    auto dev = GetDevice();
    Sampler *sampler = dev->CreateSampler(tex);
    if (sampler)
    {
        AnsiString name = "Sampler_" + AnsiString::numberOf((uint64_t)(uintptr_t)sampler);
        VkSampler vk_sampler = *sampler;
        dev->TrackObject(VK_OBJECT_TYPE_SAMPLER, (uint64_t)(uintptr_t)vk_sampler, name);
        Add(sampler);
    }
    return sampler;
}

VK_NAMESPACE_END
