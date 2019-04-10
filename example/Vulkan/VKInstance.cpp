#include"VKInstance.h"
#include<hgl/type/DataType.h>

VK_NAMESPACE_BEGIN

Instance::Instance(const UTF8String &an,Window *w)
{
    win=w;
    app_name=an;

    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = app_name.c_str();
    app_info.applicationVersion = 1;
    app_info.pEngineName = "CMGameEngine/ULRE";
    app_info.engineVersion = 1;
    app_info.apiVersion = VK_API_VERSION_1_0;

    ext_list.Add(VK_KHR_SURFACE_EXTENSION_NAME);
    ext_list.Add(win->GetVulkanSurfaceExtname());

    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = nullptr;
    inst_info.flags = 0;
    inst_info.pApplicationInfo = &app_info;
    inst_info.enabledExtensionCount = ext_list.GetCount();
    inst_info.ppEnabledExtensionNames = ext_list.GetData();
    inst_info.enabledLayerCount = 0;
    inst_info.ppEnabledLayerNames = nullptr;

    inst=nullptr;
}

Instance::~Instance()
{
    physical_devices.Clear();

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

    {
        uint32_t gpu_count = 1;
        res = vkEnumeratePhysicalDevices(inst, &gpu_count, nullptr);

        if(res!=VK_SUCCESS)
            return(false);

        physical_devices.SetCount(gpu_count);
        vkEnumeratePhysicalDevices(inst, &gpu_count,physical_devices.GetData());
    }

    return(true);
}

RenderSurface *Instance::CreateRenderSurface(int pd_index)
{
    VkPhysicalDevice pd;

    if(!physical_devices.Get(pd_index,pd))
        return(false);

    return(new RenderSurface(win,inst,pd));
}

VK_NAMESPACE_END
