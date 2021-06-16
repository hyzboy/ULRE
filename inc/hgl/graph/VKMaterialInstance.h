#ifndef HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
class MaterialInstance
{
    Material *material;

    MaterialParameters *mp_value;

private:

    friend class Material;

    MaterialInstance(Material *,MaterialParameters *);

public:

    virtual ~MaterialInstance();

    Material *GetMaterial(){return material;}

    MaterialParameters *GetMP(){return mp_value;}
    MaterialParameters *GetMP(const DescriptorSetsType &type);
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INSTANCE_INCLUDE
