#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKSurfaceExtensionName.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDebugOut.h>
#include<iostream>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

void CheckInstanceLayer(CharPointerList &layer_list,CreateInstanceLayerInfo *layer_info);

Instance *CreateInstance(const AnsiString &app_name,VKDebugOut *out,CreateInstanceLayerInfo *layer_info)
{
    ApplicationInfo app_info;
    InstanceCreateInfo inst_info(&app_info);
    CharPointerList ext_list;
    CharPointerList layer_list;

    app_info.pApplicationName   = app_name.c_str();
    app_info.applicationVersion = 1;
    app_info.pEngineName        = "CMGameEngine/ULRE";
    app_info.engineVersion      = 1;
    app_info.apiVersion         = VK_API_VERSION_1_0;

    ext_list.Add(VK_KHR_SURFACE_EXTENSION_NAME);
    ext_list.Add(HGL_VK_SURFACE_EXTENSION_NAME);            //此宏在VKSurfaceExtensionName.h中定义

#ifdef _DEBUG
    ext_list.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    ext_list.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif//_DEBUG

    if(layer_info)
        CheckInstanceLayer(layer_list,layer_info);

    inst_info.enabledExtensionCount     = ext_list.GetCount();
    inst_info.ppEnabledExtensionNames   = ext_list.GetData();
    inst_info.enabledLayerCount         = layer_list.GetCount();
    inst_info.ppEnabledLayerNames       = layer_list.GetData();

    VkInstance inst;

    if(vkCreateInstance(&inst_info,nullptr,&inst)==VK_SUCCESS)
    {
#ifdef _DEBUG
        if(!out)
            out=new VKDebugOut;
#endif//_DEBUG

        if(out)
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
