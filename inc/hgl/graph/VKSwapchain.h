#pragma once

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN

struct SwapchainImage
{
    Texture2D *                    color            =nullptr;
    Texture2D *                    depth            =nullptr;

    Framebuffer *                  fbo              =nullptr;

    RenderCmdBuffer *              cmd_buf          =nullptr;

public:

    ~SwapchainImage();
};//struct SwapchainImage

struct Swapchain
{
public:

    VkDevice                        device          =VK_NULL_HANDLE;
    
    VkExtent2D                      extent;
    VkSurfaceTransformFlagBitsKHR   transform;

    VkSwapchainKHR                  swap_chain      =VK_NULL_HANDLE;
    VkSurfaceFormatKHR              surface_format {};
    VkFormat                        depth_format    =VK_FORMAT_UNDEFINED;

    uint32_t                        image_count     =0;

    SwapchainImage *                sc_image        =nullptr;

public:

    virtual ~Swapchain();
};//struct Swapchain
VK_NAMESPACE_END
