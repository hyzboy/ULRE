#ifndef HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE
#define HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE

#include<hgl/graph/vulkan/VK.h>

VK_NAMESPACE_BEGIN

class VKDebugOut
{
    VkInstance inst=nullptr;

    VkDebugUtilsMessengerEXT debug_messenger;
    VkDebugReportCallbackEXT debug_report_callback;

public:

    VKDebugOut();
    virtual ~VKDebugOut();

    virtual bool Init(VkInstance);
};//class VKDebugOut
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE
