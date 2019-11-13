#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKSurfaceExtensionName.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDebugOut.h>
#include<iostream>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

Instance *CreateInstance(const UTF8String &app_name,VKDebugOut *out)
{
    VkApplicationInfo app_info;
    VkInstanceCreateInfo inst_info;
    CharPointerList ext_list;
    CharPointerList layer_list;

    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext              = nullptr;
    app_info.pApplicationName   = app_name.c_str();
    app_info.applicationVersion = 1;
    app_info.pEngineName        = "CMGameEngine/ULRE";
    app_info.engineVersion      = 1;
    app_info.apiVersion         = VK_API_VERSION_1_0;

    ext_list.Add(VK_KHR_SURFACE_EXTENSION_NAME);
    ext_list.Add(HGL_VK_SURFACE_EXTENSION_NAME);            //此宏在VKSurfaceExtensionName.h中定义
    ext_list.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    ext_list.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    layer_list.Add("VK_LAYER_KHRONOS_validation");
    layer_list.Add("VK_LAYER_LUNARG_standard_validation");
    layer_list.Add("VK_LAYER_RENDERDOC_Capture");

    inst_info.sType                     = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext                     = nullptr;
    inst_info.flags                     = 0;
    inst_info.pApplicationInfo          = &app_info;
    inst_info.enabledExtensionCount     = ext_list.GetCount();
    inst_info.ppEnabledExtensionNames   = ext_list.GetData();
    inst_info.enabledLayerCount         = layer_list.GetCount();
    inst_info.ppEnabledLayerNames       = layer_list.GetData();

    VkInstance inst;

    if(vkCreateInstance(&inst_info,nullptr,&inst)==VK_SUCCESS)
    {
        if(!out)
            out=new VKDebugOut;
        
        out->Init(inst);
        return(new Instance(inst,out));
    }

    return(nullptr);
}

Instance::Instance(VkInstance i,VKDebugOut *out)
{
    inst=i;

    debug_out=out;

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
    SAFE_CLEAR(debug_out);
    
    physical_devices.Clear();
    vkDestroyInstance(inst,nullptr);
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
VK_NAMESPACE_END
