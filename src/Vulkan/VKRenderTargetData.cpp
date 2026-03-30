#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

bool RenderTargetData::Submit(Semaphore *wait_sem)
{
//    std::cerr << "[RenderTargetData] Submit queue=" << queue << " cmd_buf=" << cmd_buf << " wait_sem=" << wait_sem << " render_complete=" << render_complete_semaphore << std::endl;
    if(!queue||!cmd_buf||!render_complete_semaphore)
        return(false);

    // 离屏渲染没有后续等待该信号量的提交，继续 signal 会触发“重复 signal 未等待”的校验错误。
    Semaphore *signal_sem = wait_sem ? render_complete_semaphore : nullptr;
    bool ok = queue->Submit(cmd_buf, wait_sem, signal_sem);
//    std::cerr << "[RenderTargetData] Submit result=" << ok << std::endl;
    return ok;
}

RenderCmdBuffer *RenderTargetData::BeginRender()
{
//    std::cerr << "[RenderTargetData] BeginRender cmd_buf=" << cmd_buf << " fbo=" << fbo << std::endl;
    if(!cmd_buf)
        return(nullptr);

    cmd_buf->Begin();
    cmd_buf->BeginSetup(this);
    return cmd_buf;
}

void RenderTargetData::EndRender()
{
//    std::cerr << "[RenderTargetData] EndRender cmd_buf=" << cmd_buf << std::endl;
    if(!cmd_buf)
        return;

    cmd_buf->End();
}

void RenderTargetData::Clear()
{
    LogDebug("[RenderTargetData] Clear");
    SAFE_CLEAR(render_complete_semaphore);
    render_format = nullptr;

    // cmd_buf and queue will be cleared separately or by their owners
    // DO NOT delete them here as they may still be referenced
    cmd_buf = nullptr;
    queue = nullptr;

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
