#ifndef HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKDescriptorSets.h>
VK_NAMESPACE_BEGIN
class MaterialParameters
{
    const ShaderModuleMap *shader_map;

    DescriptorSetsType ds_type;

    DescriptorSets *descriptor_sets;

private:

    friend class Material;

    MaterialParameters(const ShaderModuleMap *,const DescriptorSetsType &type,DescriptorSets *);

public:

    const   DescriptorSetsType  GetType             (){return ds_type;}
            DescriptorSets *    GetDescriptorSet    (){return descriptor_sets;}
    const   VkDescriptorSet     GetVkDescriptorSet  ()const{return descriptor_sets->GetDescriptorSet();}

public:

    #define MP_TYPE_IS(name)    const   bool is##name()const{return ds_type==DescriptorSetsType::name;}
        MP_TYPE_IS(Material)
//        MP_TYPE_IS(Texture)
        MP_TYPE_IS(Value)
        MP_TYPE_IS(Renderable)
        MP_TYPE_IS(Global)
    #undef MP_TYPE_IS

public:

    virtual ~MaterialParameters();

    bool BindUBO(const AnsiString &name,GPUBuffer *ubo,bool dynamic=false);
    bool BindSSBO(const AnsiString &name,GPUBuffer *ubo,bool dynamic=false);
    bool BindSampler(const AnsiString &name,Texture *tex,Sampler *sampler);
    
    void Update();
};//class MaterialParameters
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE