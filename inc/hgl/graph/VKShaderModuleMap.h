#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_MAP_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_MAP_INCLUDE

#include<hgl/type/Map.h>
#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN

using namespace hgl;

class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
{
public:

    using Map<VkShaderStageFlagBits,const ShaderModule *>::Map;
    ~ShaderModuleMap()=default;

    bool Add(const ShaderModule *sm);
};//class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_MAP_INCLUDE
