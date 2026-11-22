#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>
#include<cstring>

namespace
{
    class EmptyTarget final : public hgl::graph::IRenderTarget
    {
    public:
        EmptyTarget():IRenderTarget(nullptr,VkExtent2D{1,1}){}
        uint32_t GetColorCount() override { return 1; }
        bool hasDepth() override { return false; }
        hgl::graph::Framebuffer *GetFramebuffer() override { return nullptr; }
        hgl::graph::RenderPass *GetRenderPass() override { return nullptr; }
        hgl::graph::Texture2D *GetColorTexture(int) override { return nullptr; }
        hgl::graph::Texture2D *GetDepthTexture() override { return nullptr; }
        hgl::graph::DeviceQueue *GetQueue() override { return nullptr; }
        hgl::graph::RenderCmdBuffer *GetRenderCmdBuffer() override { return nullptr; }
        bool Submit(const hgl::graph::SemaphoreSubmit *,uint32_t) override { return false; }
        bool WaitQueue() override { return false; }
        bool WaitFence() override { return false; }
        hgl::graph::RenderCmdBuffer *BeginRender() override { return nullptr; }
        void EndRender() override {}
    };
}

int main()
{
    using hgl::graph::FrameDebugger;
    EmptyTarget target;
    if(FrameDebugger::RequestColor(&target,"../unsafe")!=UINT32_MAX)
    {
        GLogError(u8"Unsafe capture name was accepted");
        return 1;
    }
    if(FrameDebugger::RequestColor(&target,"missing") == UINT32_MAX)
    {
        GLogError(u8"Valid capture request was rejected");
        return 1;
    }
    if(FrameDebugger::CollectPending()!=0)
    {
        GLogError(u8"Empty target was reported as a successful capture");
        return 1;
    }
    const FrameDebugger::CaptureResult *first=FrameDebugger::GetResult(0);
    if(!first || first->valid || !first->error
       || ::strcmp(first->error,"readback_failed")!=0)
    {
        GLogError(u8"Readback failure was not recorded");
        return 1;
    }
    const uint64_t first_id=first->capture_id;
    if(FrameDebugger::RequestColor(&target,"missing2")==UINT32_MAX
       || FrameDebugger::CollectPending()!=0)
        return 1;
    const FrameDebugger::CaptureResult *second=FrameDebugger::GetResult(1);
    if(!second || second->capture_id<=first_id
       || !::strstr(FrameDebugger::GetLatestResultJSON().c_str(),"readback_failed"))
    {
        GLogError(u8"Latest failure did not identify the new capture");
        return 1;
    }
    return 0;
}
