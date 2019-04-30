#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKSurfaceExtensionName.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<iostream>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

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

Instance *CreateInstance(const UTF8String &app_name)
{
    VkApplicationInfo app_info;
    VkInstanceCreateInfo inst_info;
    CharPointerList ext_list;

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = app_name.c_str();
    app_info.applicationVersion = 1;
    app_info.pEngineName = "CMGameEngine/ULRE";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;

    ext_list.Add(VK_KHR_SURFACE_EXTENSION_NAME);
    ext_list.Add(HGL_VK_SURFACE_EXTENSION_NAME);            //此宏在VKSurfaceExtensionName.h中定义
    ext_list.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    ext_list.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    const char *validation_layers[]=
    {
//        "VK_LAYER_LUNARG_api_dump",
//        "VK_LAYER_LUNARG_assistant_layer",
        "VK_LAYER_LUNARG_core_validation",
//        "VK_LAYER_LUNARG_device_simulation",
//        "VK_LAYER_LUNARG_monitor",
        "VK_LAYER_LUNARG_object_tracker",
        "VK_LAYER_LUNARG_standard_validation",
        "VK_LAYER_LUNARG_parameter_validation",
//        "VK_LAYER_LUNARG_vktrace",
//        "VK_LAYER_RENDERDOC_Capture",

//        "VK_LAYER_KHRONOS_validation",

//        "VK_LAYER_NV_nsight-sys",

        "VK_LAYER_GOOGLE_unique_objects",
        "VK_LAYER_GOOGLE_threading"
    };

    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = ext_list.GetCount();
    inst_info.ppEnabledExtensionNames = ext_list.GetData();
    inst_info.enabledLayerCount = sizeof(validation_layers)/sizeof(const char *);
    inst_info.ppEnabledLayerNames = validation_layers;

    VkInstance inst;

    if(vkCreateInstance(&inst_info,nullptr,&inst)==VK_SUCCESS)
        return(new Instance(inst,ext_list));

    return(nullptr);
}

Instance::Instance(VkInstance i,CharPointerList &el)
{
    inst=i;
    ext_list=el;

    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount,nullptr);

        layer_properties.SetCount(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount,layer_properties.GetData());

        debug_out(layer_properties);
    }

    {
        uint32_t prop_count;
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,nullptr);

        extension_properties.SetCount(prop_count);
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,extension_properties.GetData());

        debug_out(extension_properties);
    }

    debug_report_callback=nullptr;
    {
        VkDebugReportCallbackCreateInfoEXT create_info={};

        create_info.sType=VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        create_info.pNext=nullptr;
        create_info.flags=VK_DEBUG_REPORT_ERROR_BIT_EXT|VK_DEBUG_REPORT_WARNING_BIT_EXT|VK_DEBUG_REPORT_DEBUG_BIT_EXT|VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        create_info.pfnCallback=dbgFunc;
        create_info.pUserData=nullptr;

        CreateDebugReportCallbackEXT(inst,&create_info,nullptr,&debug_report_callback);
    }

    debug_messenger=nullptr;
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo={};
        createInfo.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback=debugCallback;

        CreateDebugUtilsMessengerEXT(inst,&createInfo,nullptr,&debug_messenger);
    }

    uint32_t gpu_count = 1;

    if(vkEnumeratePhysicalDevices(inst, &gpu_count, nullptr)==VK_SUCCESS)
    {
        VkPhysicalDevice *pd_list=new VkPhysicalDevice[gpu_count];
        vkEnumeratePhysicalDevices(inst, &gpu_count,pd_list);

        for(uint32_t i=0;i<gpu_count;i++)
            physical_devices.Add(new PhysicalDevice(inst,pd_list[i]));

        delete[] pd_list;
    }
}

Instance::~Instance()
{
    physical_devices.Clear();

    if(debug_messenger)
        DestroyDebugUtilsMessengerEXT(inst,debug_messenger,nullptr);

    if(debug_report_callback)
        DestroyDebugReportCallbackEXT(inst,debug_report_callback,nullptr);

    vkDestroyInstance(inst,nullptr);
}

const bool Instance::CheckLayerSupport(const UTF8String &layer_name)const
{
    const uint32_t count=layer_properties.GetCount();
    VkLayerProperties *lp=layer_properties.GetData();

    for(uint32_t i=0;i<count;i++)
    {
        if(layer_name==lp->layerName)
            return(true);

        ++lp;
    }

    return(false);
}

const PhysicalDevice *Instance::GetDevice(VkPhysicalDeviceType type)const
{
    const uint32_t count=physical_devices.GetCount();
    PhysicalDevice **pd=physical_devices.GetData();

    for(uint32_t i=0;i<count;i++)
    {
        if((*pd)->GetDeviceType()==type)
            return(*pd);

        ++pd;
    }

    return(nullptr);
}

Device *Instance::CreateRenderDevice(Window *win,const PhysicalDevice *pd)
{
    if(!win)
        return(nullptr);

    if(!pd)pd=GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
    if(!pd)pd=GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
    if(!pd)pd=GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡

    if(!pd)
        return(nullptr);

    return VK_NAMESPACE::CreateRenderDevice(inst,pd,win);
}
VK_NAMESPACE_END
