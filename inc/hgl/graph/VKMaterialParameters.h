#ifndef HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_PARAMETERS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>
VK_NAMESPACE_BEGIN
class MaterialParameters
{
    Material *material;

    DescriptorSetsType ds_type;

    DescriptorSets *descriptor_sets;

private:

    friend class Material;

    MaterialParameters(Material *,const DescriptorSetsType &type,DescriptorSets *);

public:

            Material *          GetMaterial      (){return material;}
    const   DescriptorSetsType  GetType          (){return ds_type;}
            DescriptorSets *    GetDescriptorSets(){return descriptor_sets;}

public:

    #define MP_TYPE_IS(name)    const   bool is##name()const{return ds_type==DescriptorSetsType::name;}
        MP_TYPE_IS(Material)
//        MP_TYPE_IS(Texture)
        MP_TYPE_IS(Values)
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