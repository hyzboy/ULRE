#ifndef HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE
#define HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

const char *GetVkDebugReportObjectTypename(VkDebugReportObjectTypeEXT objType);

/**
 * Vulkan 调试信息输出基类<br>
 * 如若需要自定义的调试信息输出，请从此类派生
 */
class VKDebugOut
{
    VkInstance inst;

    VkDebugUtilsMessengerEXT debug_messenger;
    VkDebugReportCallbackEXT debug_report_callback;

public:

    virtual VkBool32 OnDebugUtilsMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,VkDebugUtilsMessageTypeFlagsEXT messageType,const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData);
    virtual VkBool32 OnDebugReport(VkDebugReportFlagsEXT msgFlags,VkDebugReportObjectTypeEXT objType,uint64_t srcObject,size_t location,int32_t msgCode,const char *pLayerPrefix,const char *pMsg);

public:

    VKDebugOut()
    {
        inst=VK_NULL_HANDLE;
        debug_messenger=VK_NULL_HANDLE;
        debug_report_callback=VK_NULL_HANDLE;
    }

    virtual ~VKDebugOut();

    virtual bool Init(VkInstance);
};//class VKDebugOut

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_OUT_INCLUDE
