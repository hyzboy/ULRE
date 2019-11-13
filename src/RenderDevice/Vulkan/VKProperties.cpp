#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKInstance.h>

VK_NAMESPACE_BEGIN

namespace
{
    static List<VkLayerProperties> layer_properties;
    static List<VkExtensionProperties> extension_properties;
}//namespace

const List<VkLayerProperties> &GetLayerProperties(){return layer_properties;}
const List<VkExtensionProperties> &GetExtensionProperties(){return extension_properties;}

void InitVulkanProperties()
{
    layer_properties.Clear();
    extension_properties.Clear();

    {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count,nullptr);

        layer_properties.SetCount(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count,layer_properties.GetData());

        debug_out(layer_properties);
    }

    {
        uint32_t prop_count;
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,nullptr);

        extension_properties.SetCount(prop_count);
        vkEnumerateInstanceExtensionProperties(nullptr,&prop_count,extension_properties.GetData());

        debug_out(extension_properties);
    }
}

const bool CheckLayerSupport(const char *layer_name)
{
    if(!layer_name||!*layer_name)
        return(false);

    const uint32_t count=layer_properties.GetCount();
    VkLayerProperties *lp=layer_properties.GetData();

    for(uint32_t i=0;i<count;i++)
    {
        if(strcmp(layer_name,lp->layerName)==0)
            return(true);

        ++lp;
    }

    return(false);
}

void CheckInstanceLayer(CharPointerList &layer_list,CreateInstanceLayerInfo *layer_info)
{
    #define VK_LAYER_CHECK(sname,lname,name)    if(layer_info->sname.name)  \
                                                {   \
                                                    if(CheckLayerSupport("VK_LAYER_" lname "_" #name)) \
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

    VK_LAYER_KHRONOS_ADD(validation)

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
VK_NAMESPACE_END
