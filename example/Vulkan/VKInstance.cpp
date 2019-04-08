#include"VKInstance.h"
#include<hgl/type/DataType.h>

VK_NAMESPACE_BEGIN

Instance::Instance(const UTF8String &an)
{
    app_name=an;

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = app_name.c_str();
    app_info.applicationVersion = 1;
    app_info.pEngineName = "CMGameEngine/ULRE";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;

    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = 0;
    inst_info.ppEnabledExtensionNames = nullptr;
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;

    inst=nullptr;
}

Instance::~Instance()
{
    if(inst)
        vkDestroyInstance(inst,nullptr);
}

bool Instance::Init()
{
    if(inst)
        return(false);

    VkResult res=vkCreateInstance(&inst_info,nullptr,&inst);

    if(res)
    {
        inst=nullptr;
        return(false);
    }

    return(true);
}

VK_NAMESPACE_END
