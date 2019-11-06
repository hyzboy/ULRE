#include<hgl/graph/vulkan/VK.h>
#include<iostream>

VK_NAMESPACE_BEGIN
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

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,VkDebugUtilsMessageTypeFlagsEXT messageType,const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,void *pUserData)
    {
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            std::cout<<"ERROR: "<<pCallbackData->pMessage<<std::endl;
        }
        else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            std::cout<<"WARNING: "<<pCallbackData->pMessage<<std::endl;
        }
        else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        {
            std::cout<<"INFO: "<<pCallbackData->pMessage<<std::endl;
        }
        else
        if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
        {
            std::cout<<"VERBOSE: "<<pCallbackData->pMessage<<std::endl;
        }
        else
            std::cerr<<"validation layer: "<<pCallbackData->pMessage<<std::endl;

        return VK_FALSE;
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

    VKAPI_ATTR VkBool32 VKAPI_CALL dbgFunc(VkDebugReportFlagsEXT msgFlags,VkDebugReportObjectTypeEXT objType,uint64_t srcObject,
                                           size_t location,int32_t msgCode,const char *pLayerPrefix,const char *pMsg,
                                           void *pUserData)
    {
        if(msgFlags&VK_DEBUG_REPORT_ERROR_BIT_EXT)
        {
            std::cout<<"ERROR: ";
        }
        else if(msgFlags&VK_DEBUG_REPORT_WARNING_BIT_EXT)
        {
            std::cout<<"WARNING: ";
        }
        else if(msgFlags&VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
        {
            std::cout<<"PERFORMANCE WARNING: ";
        }
        else if(msgFlags&VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
        {
            std::cout<<"INFO: ";
        }
        else if(msgFlags&VK_DEBUG_REPORT_DEBUG_BIT_EXT)
        {
            std::cout<<"DEBUG: ";
        }

        std::cout<<"["<<pLayerPrefix<<"] Code "<<msgCode<<" : "<<pMsg<<std::endl;

        /*
         * false indicates that layer should not bail-out of an
         * API call that had validation failures. This may mean that the
         * app dies inside the driver due to invalid parameter(s).
         * That's what would happen without validation layers, so we'll
         * keep that behavior here.
         */
        return false;
    }
}//namespace


    virtual bool Init(VkInstance);
    debug_report_callback=VK_NULL_HANDLE;
    {
        VkDebugReportCallbackCreateInfoEXT create_info;

        create_info.sType       =VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        create_info.pNext       =nullptr;

        create_info.flags       =VK_DEBUG_REPORT_ERROR_BIT_EXT
                                |VK_DEBUG_REPORT_WARNING_BIT_EXT
                                |VK_DEBUG_REPORT_DEBUG_BIT_EXT
                                |VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

        create_info.pfnCallback =dbgFunc;
        create_info.pUserData   =nullptr;

        CreateDebugReportCallbackEXT(inst,&create_info,nullptr,&debug_report_callback);
    }

    debug_messenger=VK_NULL_HANDLE;
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo;

        createInfo.sType            =VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.pNext            =nullptr;
        createInfo.flags            =0;

        createInfo.messageSeverity  =VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

        createInfo.messageType      =VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                                    |VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

        createInfo.pfnUserCallback  =debugCallback;
        createInfo.pUserData        =nullptr;

        CreateDebugUtilsMessengerEXT(inst,&createInfo,nullptr,&debug_messenger);
    }

    void close()
    {
        if(debug_messenger)
            DestroyDebugUtilsMessengerEXT(inst,debug_messenger,nullptr);

        if(debug_report_callback)
            DestroyDebugReportCallbackEXT(inst,debug_report_callback,nullptr);
    }