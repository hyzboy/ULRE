#include<hgl/graph/VKRenderTargetData.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKTexture.h>

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
