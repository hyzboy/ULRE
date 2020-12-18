#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>
VK_NAMESPACE_BEGIN
class MaterialInstance
{
    Material *material;

    DescriptorSets *descriptor_sets;

private:

    friend class Material;

    MaterialInstance(Material *,DescriptorSets *);

public:

    Material *      GetMaterial         (){return material;}
    DescriptorSets *GetDescriptorSets   (){return descriptor_sets;}

public:

    ~MaterialInstance();

    bool BindUBO(const AnsiString &name,GPUBuffer *ubo,bool dynamic=false);
    bool BindSampler(const AnsiString &name,Texture *tex,Sampler *sampler);
    
    void Update();
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE