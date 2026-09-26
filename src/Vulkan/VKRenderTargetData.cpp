#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

bool RenderTargetData::Submit(const SemaphoreSubmit *extra_waits,const uint32_t extra_wait_count)
{
    DeviceQueue *    queue   = GetQueue();
    RenderCmdBuffer *cmd_buf = GetCmdBuffer();

    if(!queue||!cmd_buf)
        return(false);

    // 等待列表：调用方给的额外等待（主导入方向：主帧车道、上传完成等）
    constexpr uint32_t MAX_WAITS = 16;
    SemaphoreSubmit waits[MAX_WAITS];
    uint32_t wait_count = 0;

    for(uint32_t i=0;i<extra_wait_count && wait_count<MAX_WAITS;i++)
        if(extra_waits[i].semaphore)
            waits[wait_count++] = extra_waits[i];

    // 信号：本 RT 的车道（timeline）——主帧提交会 await 本帧提交过的值（A1）。
    // timeline 允许「只 signal 无人等」，不存在二进制那种重复 signal 的非法状态。
    SemaphoreSubmit signal_item = SemaphoreSubmit::Signal(lane, NextLaneValue());

    bool ok = queue->Submit(cmd_buf, waits, wait_count, lane ? &signal_item : nullptr, lane ? 1u : 0u);
//    std::cerr << "[RenderTargetData] Submit result=" << ok << std::endl;
    return ok;
}

RenderCmdBuffer *RenderTargetData::BeginRender()
{
    if(!cmd_bufs||slot_count==0)
        return(nullptr);

    // 轮到下一个槽：复用前先等该槽自己的 fence（标准 WSI 模型；该槽从未提交过时
    // WaitLastSubmitFence 内部直接返回 true，无需额外状态）。
    //
    // 槽游标在 BeginRender 前进、而不是在 Submit 之后前进：这样 EndManagedRenderFrame
    // 提交完成后，GetRenderCompleteSemaphore() 读到的仍是**本次提交**那个槽的信号量
    // （A1 的信号量链要用它作为主帧提交的等待对象）。
    slot_index = (slot_index + 1) % slot_count;

    if(DeviceQueue *queue = GetQueue())
        queue->WaitLastSubmitFence();

    RenderCmdBuffer *cmd_buf = GetCmdBuffer();
    if(!cmd_buf)
        return(nullptr);

    cmd_buf->Begin();
    // Dynamic Rendering：不再 BindFramebuffer——附件由渲染循环经 IRenderTarget 附件接口提供
    return cmd_buf;
}

void RenderTargetData::EndRender()
{
    RenderCmdBuffer *cmd_buf = GetCmdBuffer();
    if(!cmd_buf)
        return;

    // Ensure our tracked layout matches the render pass finalLayout (SRO).
    // This covers both cases: whether or not any draw happened, the render pass
    // transitions attachments to finalLayout. We avoid inserting extra barriers here.
    if (color_count > 0 && color_textures)
    {
        for (uint32_t i = 0; i < color_count; ++i)
        {
            Texture2D *tex = color_textures[i];
            if (!tex) continue;

            TextureData *td = tex->GetData();
            if (td)
                td->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }

    // 深度同理：EndRenderingPresent 已把深度转到 SHADER_READ_ONLY_OPTIMAL
    //（与采样侧 descriptor 的 imageLayout 一致）。
    // depth-only 目标（shadow map）没有颜色附件，这一步是它唯一需要同步的布局标记。
    if (depth_texture)
    {
        TextureData *td = depth_texture->GetData();
        if (td)
            td->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    cmd_buf->End();
}

void RenderTargetData::Clear()
{
    LogDebug("[RenderTargetData] Clear");
    SAFE_CLEAR(fbo);

    // queue（含其 fence 数组）与 cmd_buf 由 CreateOffscreenRT 创建、本结构独占，
    // 仅在 OffscreenRenderTarget 析构走到这里时销毁。此前只置空不销毁，
    // 导致 fence 泄漏至 vkDestroyDevice（VUID-vkDestroyDevice-device-05137）。
    // 注意：必须在置空前销毁，且本结构无其它持有者（SwapchainRenderTarget
    // 使用自有 sync_slots，不经此处）。
    if(cmd_bufs)
    {
        for(uint32_t i=0;i<slot_count;i++)
            SAFE_CLEAR(cmd_bufs[i]);

        delete[] cmd_bufs;
        cmd_bufs = nullptr;
    }

    if(queues)
    {
        for(uint32_t i=0;i<slot_count;i++)
            SAFE_CLEAR(queues[i]);

        delete[] queues;
        queues = nullptr;
    }

    // 车道（timeline 信号量）由本结构独占，随 RT 销毁
    SAFE_CLEAR(lane);
    lane_value = 0;

    slot_count = 0;
    slot_index = 0;

    // Textures are managed by TextureManager, so just clear the pointers
    // Do NOT delete the textures themselves to avoid double deletion
    if(color_textures)
    {
        delete[] color_textures;
        color_textures = nullptr;
    }
    color_count = 0;

    depth_texture = nullptr;
}

}//namespace hgl::graph

