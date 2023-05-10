#ifndef HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE
#define HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/type/List.h>
VK_NAMESPACE_BEGIN
struct Swapchain
{
public:

    VkDevice                device          =VK_NULL_HANDLE;
    
    VkExtent2D              extent;

    VkSwapchainKHR          swap_chain      =VK_NULL_HANDLE;

    uint32_t                color_count     =0;

    Texture2D **            sc_color        =nullptr;
    Texture2D *             sc_depth        =nullptr;

    Framebuffer **          sc_fbo          =nullptr;

public:

    virtual ~Swapchain();
};//struct Swapchain
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE
