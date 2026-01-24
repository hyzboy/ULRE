#include<hgl/graph/VK.h>
#include<hgl/graph/VKInstance.h>
#include"DebugOutProperties.h"

VK_NAMESPACE_BEGIN

namespace
{
    static ValueArray<VkLayerProperties> layer_properties;
    static ValueArray<VkExtensionProperties> extension_properties;
}//namespace

const ValueArray<VkLayerProperties> &GetLayerProperties(){return layer_properties;}
const ValueArray<VkExtensionProperties> &GetExtensionProperties(){return extension_properties;}

void InitVulkanInstanceProperties()
{
    layer_properties.Clear();
    extension_properties.Clear();

    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count,nullptr);

        layer_properties.Resize(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count,layer_properties.GetData());

        debug_out("Instance",layer_properties);
    }

    {
        uint32_t prop_count;
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,nullptr);

        extension_properties.Resize(prop_count);
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,extension_properties.GetData());

        debug_out("Instance",extension_properties);
    }
}

const bool CheckInstanceLayerSupport(const AnsiString &layer_name)
{
    if(!layer_name||!*layer_name)
        return(false);

    for(const VkLayerProperties &lp:layer_properties)
        if(layer_name.Comp(lp.layerName)==0)
            return(true);

    return(false);
}

const bool GetInstanceLayerVersion(const AnsiString &name,uint32_t &spec,uint32_t &impl)
{    
    for(const VkLayerProperties &lp:layer_properties)
    {
        if(name.Comp(lp.layerName)==0)
        {
            spec=lp.specVersion;
            impl=lp.implementationVersion;

            return(true);
        }
    }

    return(false);
}

void CheckInstanceLayer(CharPointerList &layer_list,CreateInstanceLayerInfo *layer_info)
{
    #define VK_LAYER_CHECK(sname,lname,name)    if(layer_info->sname.name)  \
                                                {   \
                                                    if(CheckInstanceLayerSupport("VK_LAYER_" lname "_" #name)) \
                                                          layer_list.Add("VK_LAYER_" lname "_" #name); \
                                                }

#define VK_LAYER_LUNARG_ADD(name)       VK_LAYER_CHECK(lunarg,"LUNARG",name)

    VK_LAYER_LUNARG_ADD(api_dump)
    VK_LAYER_LUNARG_ADD(device_simulation)
    VK_LAYER_LUNARG_ADD(monitor)
    VK_LAYER_LUNARG_ADD(screenshot)
    VK_LAYER_LUNARG_ADD(standard_validation)
    VK_LAYER_LUNARG_ADD(vktrace)

#define VK_LAYER_KHRONOS_ADD(name)      VK_LAYER_CHECK(khronos,"KHRONOS",name)

    VK_LAYER_KHRONOS_ADD(synchronization2)
    VK_LAYER_KHRONOS_ADD(validation)
    VK_LAYER_KHRONOS_ADD(profiles)

#define VK_LAYER_AMD_ADD(name)          VK_LAYER_CHECK(amd,"AMD",name)

    VK_LAYER_AMD_ADD(switchable_graphics)

#define VK_LAYER_NV_ADD(name)           VK_LAYER_CHECK(nv,"NV",name)

    VK_LAYER_NV_ADD(optimus)

#define VK_LAYER_VALVE_ADD(name)        VK_LAYER_CHECK(valve,"VALVE",name)

    VK_LAYER_VALVE_ADD(steam_overlay)
    VK_LAYER_VALVE_ADD(steam_fossilize)

#define VK_LAYER_RENDERDOC_ADD(name)    VK_LAYER_CHECK(RenderDoc,"RENDERDOC",name)

    VK_LAYER_RENDERDOC_ADD(Capture)

#define VK_LAYER_BANDICAM_ADD(name)     VK_LAYER_CHECK(bandicam,"bandicam",name)

    VK_LAYER_BANDICAM_ADD(helper)    
}

const bool CheckInstanceExtensionSupport(const AnsiString &name)
{
    for(const VkExtensionProperties &ep:extension_properties)
    {
        if(name.Comp(ep.extensionName)==0)
            return true;
    }

    return(false);
}
VK_NAMESPACE_END
