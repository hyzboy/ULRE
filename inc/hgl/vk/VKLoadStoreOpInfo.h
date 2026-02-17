#ifndef HGL_VULKAN_LOAD_STORE_OP_INFO_INCLUDE
#define HGL_VULKAN_LOAD_STORE_OP_INFO_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
struct LoadStoreInfo
{
    VkAttachmentLoadOp  load_op     = VK_ATTACHMENT_LOAD_OP_CLEAR;
    VkAttachmentStoreOp store_op    = VK_ATTACHMENT_STORE_OP_STORE;
};
}//namespace hgl::graph
#endif//HGL_VULKAN_LOAD_STORE_OP_INFO_INCLUDE
