#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKDevice.h>
#include<cstdint>

namespace hgl::graph{
GRAPH_MODULE_CONSTRUCT(RenderPassManager)
{
}

void RenderPassManager::Release()
{
    std::cout << "[DEBUG] RenderPassManager::Release() - RenderPassList.GetCount()=" << RenderPassList.GetCount() << std::endl;
    if (RenderPassList.GetCount() > 0)
    {
        for (auto &kv : RenderPassList)
        {
            std::cout << "[DEBUG] RenderPassManager::Release() - Deleting RenderPass: " << kv.first.c_str() << std::endl;
            delete kv.second;
        }

        RenderPassList.Clear();
    }
    std::cout << "[DEBUG] RenderPassManager::Release() - Complete" << std::endl;
}

namespace
{
    AnsiString GenerateRenderPassKey(const RenderbufferInfo *rbi)
    {
        AnsiString key;
        hgl::Sprintf(key,
                     "RenderPass_%d_%u_%u_%u",
                     rbi->GetDepthFormat(),
                     (uint)rbi->GetColorLayout(),
                     (uint)rbi->GetDepthLayout(),
                     rbi->GetColorCount());

        for (const VkFormat &fmt : rbi->GetColorFormatList())
        {
            key += "_";
            key += AnsiString::numberOf((int)fmt);
        }

        return key;
    }
}

RenderPass *RenderPassManager::AcquireRenderPass(const RenderbufferInfo *rbi)
{
    HGL_CAPTURE_SCOPE();

    {
        const auto *phy_dev=GetPhyDevice();

        for(const VkFormat &fmt:rbi->GetColorFormatList())
            if(!phy_dev->IsColorAttachmentOptimal(fmt))
                return(nullptr);

        if(rbi->HasDepthOrStencil())
            if(!phy_dev->IsDepthAttachmentOptimal(rbi->GetDepthFormat()))
                return(nullptr);
    }

    AnsiString key=GenerateRenderPassKey(rbi);
    RenderPass *rp=nullptr;

    if(RenderPassList.Get(key,rp))
        return rp;

    rp=new RenderPass(GetDevice(),key,rbi->GetColorFormatList(),rbi->GetDepthFormat());
    RenderPassList.Add(key,rp);
    return rp;
}
}//namespace hgl::graph
