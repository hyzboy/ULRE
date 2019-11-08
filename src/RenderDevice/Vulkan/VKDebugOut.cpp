#include<hgl/graph/vulkan/VKDebugOut.h>
#include<iostream>

VK_NAMESPACE_BEGIN
namespace
{
    const char VkDebugReportObjectTypename[][32]=
    {
        "UNKNOWN",
        "INSTANCE",
        "PHYSICAL_DEVICE",
        "DEVICE",
        "QUEUE",
        "SEMAPHORE",
        "COMMAND_BUFFER",
        "FENCE",
        "DEVICE_MEMORY",
        "BUFFER",
        "IMAGE",
        "EVENT",
        "QUERY_POOL",
        "BUFFER_VIEW",
        "IMAGE_VIEW",
        "SHADER_MODULE",
        "PIPELINE_CACHE",
        "PIPELINE_LAYOUT",
        "RENDER_PASS",
        "PIPELINE",
        "DESCRIPTOR_SET_LAYOUT",
        "SAMPLER",
        "DESCRIPTOR_POOL",
        "DESCRIPTOR_SET",
        "FRAMEBUFFER",
        "COMMAND_POOL",
        "SURFACE_KHR",
        "SWAPCHAIN_KHR",
        "DEBUG_REPORT_CALLBACK_EXT",
        "DISPLAY_KHR",
        "DISPLAY_MODE_KHR",
        "OBJECT_TABLE_NVX",
        "INDIRECT_COMMANDS_LAYOUT_NVX",
        "VALIDATION_CACHE_EXT",

        "SAMPLER_YCBCR_CONVERSION",
        "DESCRIPTOR_UPDATE_TEMPLATE",
        "ACCELERATION_STRUCTURE_NV"
    };
}//namespace

const char *GetVkDebugReportObjectTypename(VkDebugReportObjectTypeEXT objType)
{
    if(objType==VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION_EXT)   return VkDebugReportObjectTypename[VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT+1];else
    if(objType==VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT) return VkDebugReportObjectTypename[VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT+2];else
    if(objType==VK_DEBUG_REPORT_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV_EXT)  return VkDebugReportObjectTypename[VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT+3];else
    if(objType<VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT
     &&objType>VK_DEBUG_REPORT_OBJECT_TYPE_VALIDATION_CACHE_EXT_EXT)        return VkDebugReportObjectTypename[0];else
                                                                            return VkDebugReportObjectTypename[objType];
}

namespace
{
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,const VkAllocationCallbacks *pAllocator,VkDebugUtilsMessengerEXT *pDebugMessenger)
    {
        auto func=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugUtilsMessengerEXT");
        if(func)
        {
            return func(instance,pCreateInfo,pAllocator,pDebugMessenger);
        }
        else
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance,VkDebugUtilsMessengerEXT debugMessenger,const VkAllocationCallbacks *pAllocator)
    {
        auto func=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugUtilsMessengerEXT");
        if(func)
        {
            func(instance,debugMessenger,pAllocator);
        }
    }

    VkBool32 DefaultVulkanDebugUtilsMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                            VkDebugUtilsMessageTypeFlagsEXT messageType,
                                            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
    {
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   std::cerr<<"[ERROR] ";           else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) std::cerr<<"[WARNING] ";         else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    std::cerr<<"[INFO] ";            else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) std::cerr<<"[VERBOSE] ";         else
                                                                            std::cerr<<"[Validation layer] ";

        std::cerr<<pCallbackData->pMessage<<std::endl;

        return VK_FALSE;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_utils_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,VkDebugUtilsMessageTypeFlagsEXT messageType,const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,void *pUserData)
    {
        if(!pCallbackData)
            return VK_FALSE;

        if(!pUserData)
            return DefaultVulkanDebugUtilsMessage(messageSeverity,messageType,pCallbackData);
        else
            return ((VKDebugOut *)pUserData)->OnDebugUtilsMessage(messageSeverity,messageType,pCallbackData);
    }

    bool CreateDebugReportCallbackEXT(VkInstance instance,const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,const VkAllocationCallbacks *pAllocator,VkDebugReportCallbackEXT *pCallback)
    {
        auto func=(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT");
        if(func)
        {
            func(instance,pCreateInfo,pAllocator,pCallback);
            return(true);
        }
        else
        {
            return(false);
        }
    }

    bool DestroyDebugReportCallbackEXT(VkInstance instance,VkDebugReportCallbackEXT callback,const VkAllocationCallbacks *pAllocator)
    {
        auto func=(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT");
        if(func)
        {
            func(instance,callback,pAllocator);
            return(true);
        }
        else
        {
            return(false);
        }
    }

    VkBool32 DefaultVulkanDebugReport(  VkDebugReportFlagsEXT msgFlags,
                                        VkDebugReportObjectTypeEXT objType,
                                        uint64_t srcObject,
                                        size_t location,
                                        int32_t msgCode,
                                        const char *pLayerPrefix,
                                        const char *pMsg)
    {
        const char *obj_type_name=GetVkDebugReportObjectTypename(objType);

        if(msgFlags&VK_DEBUG_REPORT_ERROR_BIT_EXT)              std::cerr<<"[ERROR:";               else
        if(msgFlags&VK_DEBUG_REPORT_WARNING_BIT_EXT)            std::cerr<<"[WARNING:";             else
        if(msgFlags&VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)std::cerr<<"[PERFORMANCE WARNING:"; else
        if(msgFlags&VK_DEBUG_REPORT_INFORMATION_BIT_EXT)        std::cerr<<"[INFO:";                else
        if(msgFlags&VK_DEBUG_REPORT_DEBUG_BIT_EXT)              std::cerr<<"[DEBUG:";

        std::cerr<<msgCode<<"]["<<obj_type_name<<":"<<srcObject<<"][Location:"<<location<<"]["<<pLayerPrefix<<"] "<<pMsg<<std::endl;

        return VK_FALSE;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_report_callback(VkDebugReportFlagsEXT msgFlags,VkDebugReportObjectTypeEXT objType,uint64_t srcObject,
                                           size_t location,int32_t msgCode,const char *pLayerPrefix,const char *pMsg,
                                           void *pUserData)
    {
        if(!pUserData)
            return DefaultVulkanDebugReport(msgFlags,objType,srcObject,location,msgCode,pLayerPrefix,pMsg);
        else
            return ((VKDebugOut *)pUserData)->OnDebugReport(msgFlags,objType,srcObject,location,msgCode,pLayerPrefix,pMsg);
    }
}//namespace

bool VKDebugOut::Init(VkInstance i)
{
    int count=0;

    inst=i;
    {
        VkDebugReportCallbackCreateInfoEXT create_info;

        create_info.sType       =VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        create_info.pNext       =nullptr;

        create_info.flags       =VK_DEBUG_REPORT_ERROR_BIT_EXT
                                |VK_DEBUG_REPORT_WARNING_BIT_EXT
                                |VK_DEBUG_REPORT_DEBUG_BIT_EXT
                                |VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

        create_info.pfnCallback =vulkan_debug_report_callback;
        create_info.pUserData   =this;

        if(CreateDebugReportCallbackEXT(inst,&create_info,nullptr,&debug_report_callback))
            ++count;
    }

    {
        VkDebugUtilsMessengerCreateInfoEXT create_info;

        create_info.sType           =VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.pNext           =nullptr;
        create_info.flags           =0;

        create_info.messageSeverity =VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        create_info.messageType     =VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        create_info.pfnUserCallback =vulkan_debug_utils_callback;
        create_info.pUserData       =this;

        if(CreateDebugUtilsMessengerEXT(inst,&create_info,nullptr,&debug_messenger))
            ++count;
    }

    return(count);
}

VKDebugOut::~VKDebugOut()
{
    if(debug_messenger)
        DestroyDebugUtilsMessengerEXT(inst,debug_messenger,nullptr);

    if(debug_report_callback)
        DestroyDebugReportCallbackEXT(inst,debug_report_callback,nullptr);
}

VkBool32 VKDebugOut::OnDebugUtilsMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,VkDebugUtilsMessageTypeFlagsEXT messageType,const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData)
{
    return DefaultVulkanDebugUtilsMessage(messageSeverity,messageType,pCallbackData);
}

VkBool32 VKDebugOut::OnDebugReport(VkDebugReportFlagsEXT msgFlags,VkDebugReportObjectTypeEXT objType,uint64_t srcObject,size_t location,int32_t msgCode,const char *pLayerPrefix,const char *pMsg)
{
    return DefaultVulkanDebugReport(msgFlags,objType,srcObject,location,msgCode,pLayerPrefix,pMsg);
}
VK_NAMESPACE_END
