#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKSurfaceExtensionName.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDebugOut.h>
#include<iostream>

VK_NAMESPACE_BEGIN
GPUDevice *CreateRenderDevice(VkInstance,const GPUPhysicalDevice *,Window *);

void CheckInstanceLayer(CharPointerList &layer_list,CreateInstanceLayerInfo *layer_info);

VulkanInstance *CreateInstance(const AnsiString &app_name,VKDebugOut *out,CreateInstanceLayerInfo *layer_info)
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

    constexpr char *require_ext_list[]=
    {
#ifdef _DEBUG
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif//_DEBUG
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
    };

    for(const char *ext_name:require_ext_list)
        if(CheckInstanceExtensionSupport(ext_name))
            ext_list.Add(ext_name);

    if(layer_info)
    {
        if(layer_info->khronos.validation)
            ext_list.Add(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        CheckInstanceLayer(layer_list,layer_info);
    }

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

        return(new VulkanInstance(inst,out));
    }

    return(nullptr);
}

VulkanInstance::VulkanInstance(VkInstance i,VKDebugOut *out)
{
    inst=i;

    debug_out=out;

    uint32_t gpu_count = 1;

    if(vkEnumeratePhysicalDevices(inst, &gpu_count, nullptr)==VK_SUCCESS)
    {
        VkPhysicalDevice *pd_list=new VkPhysicalDevice[gpu_count];
        vkEnumeratePhysicalDevices(inst, &gpu_count,pd_list);

        for(uint32_t i=0;i<gpu_count;i++)
            physical_devices.Add(new GPUPhysicalDevice(inst,pd_list[i]));

        delete[] pd_list;
    }

    GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(inst, "vkGetDeviceProcAddr");
}

VulkanInstance::~VulkanInstance()
{
    SAFE_CLEAR(debug_out);
    
    physical_devices.Clear();
    vkDestroyInstance(inst,nullptr);
}

const GPUPhysicalDevice *VulkanInstance::GetDevice(VkPhysicalDeviceType type)const
{
    for(GPUPhysicalDevice *pd:physical_devices)
        if(pd->GetDeviceType()==type)
            return(pd);

    return(nullptr);
}

void VulkanInstance::DestroySurface(VkSurfaceKHR surface)
{
    vkDestroySurfaceKHR(inst,surface,nullptr);
}
VK_NAMESPACE_END
