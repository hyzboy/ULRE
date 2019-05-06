#ifndef HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN
struct ShaderResourceList
{
    Map<UTF8String,int> binding_by_name;                    ///<名字索引
    List<uint32_t> binding_list;                            ///<资源binding列表

public:

    const int GetBinding(const UTF8String &name)const
    {
        int binding;

        if(binding_by_name.Get(name,binding))
            return binding;
        else
            return -1;
    }
};//struct ShaderResource

using ShaderResource=ShaderResourceList[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_RESOURCE_INCLUDE
