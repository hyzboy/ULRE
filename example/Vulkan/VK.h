#ifndef HGL_GRAPH_VULKAN_INCLUDE
#define HGL_GRAPH_VULKAN_INCLUDE

#include<hgl/type/List.h>
#include<vulkan/vulkan.h>
#include<iostream>

#define VK_NAMESPACE_BEGIN  namespace hgl{namespace graph{namespace vulkan{
#define VK_NAMESPACE_END    }}}

using CharPointerList=hgl::List<const char *>;

inline void debug_out(const hgl::List<VkLayerProperties> &layer_properties)
{
    const int property_count=layer_properties.GetCount();

    if(property_count<=0)return;

    const VkLayerProperties *lp=layer_properties.GetData();

    for(int i=0;i<property_count;i++)
    {
        std::cout<<"Layer Propertyes ["<<i<<"] : "<<lp->layerName<<" desc: "<<lp->description<<std::endl;
        ++lp;
    }
}

inline void debug_out(const hgl::List<VkExtensionProperties> &extension_properties)
{
    const int extension_count=extension_properties.GetCount();

    if(extension_count<=0)return;

    VkExtensionProperties *ep=extension_properties.GetData();
    for(int i=0;i<extension_count;i++)
    {
        std::cout<<"Extension Propertyes ["<<i<<"] : "<<ep->extensionName<<" ver: "<<ep->specVersion<<std::endl;
        ++ep;
    }
}

#endif//HGL_GRAPH_VULKAN_INCLUDE
