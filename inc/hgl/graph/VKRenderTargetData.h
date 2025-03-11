#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

struct RenderTargetData
{
    Framebuffer *       fbo;
    DeviceQueue *       queue;
    Semaphore *         render_complete_semaphore;

    RenderCmdBuffer *   cmd_buf;

    uint32_t            color_count;            ///<颜色成分数量
    Texture2D **        color_textures;         ///<颜色成分纹理列表
    Texture2D *         depth_texture;          ///<深度成分纹理

public:

    Texture2D *GetColorTexture(const uint32_t index)
    {
        if(index>=color_count)
            return(nullptr);

        return color_textures[index];
    }

    bool Submit(Semaphore *wait_sem);

    RenderCmdBuffer *BeginRender(DescriptorBinding *);

    void EndRender();

    virtual void Clear();
};//struct RenderTargetData

VK_NAMESPACE_END
