#ifndef HGL_GRAPH_VULKAN_SWAP_CHAIN_ATTRIBUTE_INCLUDE
#define HGL_GRAPH_VULKAN_SWAP_CHAIN_ATTRIBUTE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/List.h>
VK_NAMESPACE_BEGIN
struct SwapchainAttribute
{
    VkDevice                device          =VK_NULL_HANDLE;
    
    VkExtent2D              extent;

    VkQueue                 graphics_queue  =VK_NULL_HANDLE;
    VkSwapchainKHR          swap_chain      =VK_NULL_HANDLE;

    uint32_t                swap_chain_count=0;

    ObjectList<Texture2D>   sc_color;
    Texture2D *             sc_depth        =nullptr;

public:
    
    ~SwapchainAttribute();
};//struct SwapchainAttribute
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SWAP_CHAIN_ATTRIBUTE_INCLUDE