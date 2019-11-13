#include<hgl/graph/vulkan/VKInstance.h>
#include<hgl/graph/vulkan/VKSurfaceExtensionName.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDebugOut.h>
#include<iostream>

VK_NAMESPACE_BEGIN
Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

using CharPointerList=hgl::List<const char *>;

Instance *CreateInstance(const UTF8String &app_name,VKDebugOut *out,CreateInstanceLayerInfo *layer_info)
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

#ifdef _DEBUG
    ext_list.Add(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    ext_list.Add(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif//_DEBUG

    if(layer_info)
    {
    #define VK_LAYER_LUNARG_ADD(name)       if(layer_info->lunarg.name)layer_list.Add("VK_LAYER_LUNARG_" #name);

        VK_LAYER_LUNARG_ADD(api_dump)
        VK_LAYER_LUNARG_ADD(device_simulation)
        VK_LAYER_LUNARG_ADD(monitor)
        VK_LAYER_LUNARG_ADD(screenshot)
        VK_LAYER_LUNARG_ADD(standard_validation)
        VK_LAYER_LUNARG_ADD(vktrace)

    #define VK_LAYER_KHRONOS_ADD(name)      if(layer_info->khronos.name)layer_list.Add("VK_LAYER_KHRONOS_" #name);

        VK_LAYER_KHRONOS_ADD(validation)

    #define VK_LAYER_NV_ADD(name)           if(layer_info->nv.name)layer_list.Add("VK_LAYER_NV_" #name);

        VK_LAYER_NV_ADD(optimus)

    #define VK_LAYER_VALVE_ADD(name)        if(layer_info->valve.name)layer_list.Add("VK_LAYER_VALVE_" #name);

        VK_LAYER_VALVE_ADD(steam_overlay)
        VK_LAYER_VALVE_ADD(steam_fossilize)

    #define VK_LAYER_RENDERDOC_ADD(name)    if(layer_info->RenderDoc.name)layer_list.Add("VK_LAYER_RENDERDOC_" #name);

        VK_LAYER_RENDERDOC_ADD(Capture)

    #define VK_LAYER_BANDICAM_ADD(name)     if(layer_info->bandicam.name)layer_list.Add("VK_LAYER_bandicam_" #name);

        VK_LAYER_BANDICAM_ADD(helper)
    }

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
