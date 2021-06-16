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

public:

    const int                   GetBinding(VkDescriptorType,const AnsiString &)const;

#define GET_BO_BINDING(name,vk_name)    const int Get##name(const AnsiString &obj_name)const{return GetBinding(VK_DESCRIPTOR_TYPE_##vk_name,obj_name);}
//        GET_BO_BINDING(Sampler,             SAMPLER)

        GET_BO_BINDING(Sampler,             COMBINED_IMAGE_SAMPLER)
//        GET_BO_BINDING(SampledImage,        SAMPLED_IMAGE)
        GET_BO_BINDING(StorageImage,        STORAGE_IMAGE)

        GET_BO_BINDING(UTBO,                UNIFORM_TEXEL_BUFFER)
        GET_BO_BINDING(SSTBO,               STORAGE_TEXEL_BUFFER)
        GET_BO_BINDING(UBO,                 UNIFORM_BUFFER)
        GET_BO_BINDING(SSBO,                STORAGE_BUFFER)

        //shader中并不区分普通UBO和动态UBO，所以Material/ShaderResource中的数据，只有UBO

//        GET_BO_BINDING(UBODynamic,          UNIFORM_BUFFER_DYNAMIC)
//        GET_BO_BINDING(SSBODynamic,         STORAGE_BUFFER_DYNAMIC)

        GET_BO_BINDING(InputAttachment,     INPUT_ATTACHMENT)
    #undef GET_BO_BINDING
};//class ShaderModuleMap:public Map<VkShaderStageFlagBits,const ShaderModule *>
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_MAP_INCLUDE
