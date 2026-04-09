#include<vulkan/vulkan.h>
#include<iostream>
#include<vector>

namespace hgl::graph{

inline void debug_out_vk_version(const uint32_t version)
{
    std::cout<<VK_VERSION_MAJOR(version)<<"."
             <<VK_VERSION_MINOR(version)<<"."
             <<VK_VERSION_PATCH(version);
}

inline void debug_out_layers(const char *front,const std::vector<VkLayerProperties> &layer_properties)
{
    const int property_count=static_cast<int>(layer_properties.size());

    if(property_count<=0)return;

    const VkLayerProperties *lp=layer_properties.data();

    for(int i=0;i<property_count;i++)
    {
        std::cout<<front<<" Layer Propertyes ["<<i<<"] : "<<lp->layerName<<" [spec: ";
        debug_out_vk_version(lp->specVersion);

        std::cout<<", impl: ";
        debug_out_vk_version(lp->implementationVersion);

        std::cout<<"] desc: "<<lp->description<<std::endl;
        ++lp;
    }
}

inline void debug_out_extensions(const char *front,const std::vector<VkExtensionProperties> &extension_properties)
{
    const int extension_count=static_cast<int>(extension_properties.size());

    if(extension_count<=0)return;

    const VkExtensionProperties *ep=extension_properties.data();
    for(int i=0;i<extension_count;i++)
    {
        std::cout<<front<<" Extension Propertyes ["<<i<<"] : "<<ep->extensionName<<" ver: ";

        debug_out_vk_version(ep->specVersion);

        std::cout<<std::endl;
        ++ep;
    }
}
}//namespace hgl::graph
