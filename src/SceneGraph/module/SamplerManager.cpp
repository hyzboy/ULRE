#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/RenderFramework.h>
#include <hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CONSTRUCT(SamplerManager)
{
}

Sampler *SamplerManager::CreateSampler(VkSamplerCreateInfo *sci)
{
    auto dev = GetRenderFramework()->GetDevice();
    Sampler *sampler = dev->CreateSampler(sci);
    if (sampler)
        Add(sampler);
    return sampler;
}

Sampler *SamplerManager::CreateSampler(Texture *tex)
{
    auto dev = GetRenderFramework()->GetDevice();
    Sampler *sampler = dev->CreateSampler(tex);
    if (sampler)
        Add(sampler);
    return sampler;
}

VK_NAMESPACE_END
