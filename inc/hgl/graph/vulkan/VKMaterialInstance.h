#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/BaseString.h>
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

    bool BindUBO(const UTF8String &name,vulkan::Buffer *ubo);
    bool BindSampler(const UTF8String &name,Texture *tex,Sampler *sampler);
    
    void Update();
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE