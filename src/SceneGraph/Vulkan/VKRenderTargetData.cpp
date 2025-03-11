#include<hgl/graph/VKRenderTargetData.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>

VK_NAMESPACE_BEGIN

bool RenderTargetData::Submit(Semaphore *wait_sem)
{
    if(!queue||!cmd_buf||!render_complete_semaphore)
        return(false);

    return queue->Submit(cmd_buf,wait_sem,render_complete_semaphore);
}

RenderCmdBuffer *RenderTargetData::BeginRender(DescriptorBinding *db)
{
    if(!cmd_buf)
        return(nullptr);

    cmd_buf->Begin();
    cmd_buf->SetDescriptorBinding(db);
    cmd_buf->BindFramebuffer(fbo);
    return cmd_buf;
}

void RenderTargetData::EndRender()
{
    if(!cmd_buf)
        return;

    cmd_buf->End();
}

void RenderTargetData::Clear()
{
    SAFE_CLEAR(queue);
    SAFE_CLEAR(render_complete_semaphore);
    SAFE_CLEAR(fbo);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(color_textures,color_count);
    SAFE_CLEAR(depth_texture);
}

VK_NAMESPACE_END
