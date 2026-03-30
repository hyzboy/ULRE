#pragma once

#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

struct RenderTargetData
{
    OBJECT_LOGGER

    Framebuffer *       fbo;
    DeviceQueue *       queue;
    Semaphore *         render_complete_semaphore;

    RenderCmdBuffer *   cmd_buf;

    uint32_t            color_count;            ///<颜色成分数量
    Texture2D **        color_textures;         ///<颜色成分纹理列表
    Texture2D *         depth_texture;          ///<深度成分纹理

    // Target layout for color attachments after rendering.
    // Swapchain: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    // Offscreen: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (default)
    VkImageLayout       final_color_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

public:

    Texture2D *GetColorTexture(const uint32_t index)
    {
        if(index>=color_count)
            return(nullptr);

        return color_textures[index];
    }

    bool Submit(Semaphore *wait_sem);

    RenderCmdBuffer *BeginRender();

    void EndRender();

    virtual void Clear();
};//struct RenderTargetData

}//namespace hgl::graph
