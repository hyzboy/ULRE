#pragma once

#include<hgl/vk/VK.h>
namespace hgl::graph{

struct SwapchainImage
{
    Texture2D *                    color            =nullptr;
    Texture2D *                    depth            =nullptr;

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
}//namespace hgl::graph
