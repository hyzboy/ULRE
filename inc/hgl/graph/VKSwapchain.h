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

    VkQueue                 graphics_queue  =VK_NULL_HANDLE;
    VkSwapchainKHR          swap_chain      =VK_NULL_HANDLE;

    uint32_t                swap_chain_count=0;

    ObjectList<Texture2D>   sc_color;
    Texture2D *             sc_depth        =nullptr;

public:

            VkSwapchainKHR  GetSwapchain            ()          {return swap_chain;}
    const   VkExtent2D &    GetExtent               ()const     {return extent;}
    const   uint32_t        GetImageCount           ()const     {return sc_color.GetCount();}

            Texture2D **    GetColorTextures        ()          {return sc_color.GetData();}
            Texture2D *     GetColorTexture         (int index) {return sc_color[index];}
            Texture2D *     GetDepthTexture         ()          {return sc_depth;}

public:

    virtual ~Swapchain();
};//struct Swapchain
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE
