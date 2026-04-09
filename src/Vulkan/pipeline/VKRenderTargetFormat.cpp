#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>
#include<cstdio>
#include<atomic>
#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/log/Log.h>

namespace {
    std::atomic<uint64_t> g_rf_vkcreate_count{0};
} // anonymous namespace

namespace hgl::graph{

RenderTargetFormat::RenderTargetFormat(VulkanDevice *dev,const AnsiString &n,const VkFormatList &cf,VkFormat df)
{
    device=dev;
    name=n;
    pipeline_cache=dev->GetPipelineCache();
    color_formats=cf;
    depth_format=df;

    LogInfo("[RenderTargetFormat::RenderTargetFormat] Created RenderTargetFormat '%s', color attachment count=%u, depth format=%u",
             name.c_str(), static_cast<uint32_t>(color_formats.size()), depth_format);
}

RenderTargetFormat::~RenderTargetFormat()
{
    LogInfo("[RenderTargetFormat::~RenderTargetFormat] Destroying RenderTargetFormat '%s' (RenderTargetFormat*=0x%llx)",
             name.c_str(), (unsigned long long)(uintptr_t)this);
}

uint64_t RenderTargetFormat::GetVkCreateCount()
{
    return g_rf_vkcreate_count.load(std::memory_order_relaxed);
}

void RenderTargetFormat::IncrVkCreateCount()
{
    ++g_rf_vkcreate_count;
}
}//namespace hgl::graph
