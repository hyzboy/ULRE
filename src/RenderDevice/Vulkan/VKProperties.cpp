#include<hgl/graph/vulkan/VK.h>

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
VK_NAMESPACE_END
