#include"VKInstance.h"
#include"VKSurfaceExtensionName.h"
#include<hgl/type/DataType.h>

VK_NAMESPACE_BEGIN
RenderSurface *CreateRenderSuface(VkInstance,VkPhysicalDevice,Window *);

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
    ext_list.Add(HGL_VK_SURFACE_EXTENSION_NAME);            //此宏在CMAKE中定义

    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = ext_list.GetCount();
    inst_info.ppEnabledExtensionNames = ext_list.GetData();
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;

    VkInstance inst;

    if(vkCreateInstance(&inst_info,nullptr,&inst)==VK_SUCCESS)
        return(new Instance(inst,ext_list));

    return(nullptr);
}

Instance::Instance(VkInstance i,CharPointerList &el)
{
    inst=i;
    ext_list=el;

    uint32_t gpu_count = 1;

    if(vkEnumeratePhysicalDevices(inst, &gpu_count, nullptr)==VK_SUCCESS)
    {
        physical_devices.SetCount(gpu_count);
        vkEnumeratePhysicalDevices(inst, &gpu_count,physical_devices.GetData());
    }
}

Instance::~Instance()
{
    physical_devices.Clear();

    vkDestroyInstance(inst,nullptr);
}

RenderSurface *Instance::CreateSurface(Window *win,int pd_index)
{
    if(!win)
        return(nullptr);

    VkPhysicalDevice pd=GetDevice(pd_index);

    if(!pd)
        return(nullptr);

    return CreateRenderSuface(inst,pd,win);
}
VK_NAMESPACE_END
