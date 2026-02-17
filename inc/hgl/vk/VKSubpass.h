#ifndef HGL_GRAPH_VULKAN_SUBPASS_INCLUDE
#define HGL_GRAPH_VULKAN_SUBPASS_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
/**
 * 渲染流程中一次具体的操作，即便整个Renderpass只有一次渲染，也需要创建subpass
 */
class Subpass
{
    VkSubpassDescription *desc;
    VkSubpassDependency *dependency;

    ValueArray<VkAttachmentReference> ar_input;
    ValueArray<VkAttachmentReference> ar_out_colors;
    VkAttachmentReference       ar_out_depth;

public:

    Subpass(VkSubpassDescription *sd,VkSubpassDependency *dep)
    {
        desc=sd;
        dependency=dep;
    }

    virtual ~Subpass()=default;

    const VkSubpassDescription *GetDescription  ()const{return desc;}
    const VkSubpassDependency * GetDependency   ()const{return dependency;}
};//class Subpass
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_SUBPASS_INCLUDE
