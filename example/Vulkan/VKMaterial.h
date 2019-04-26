#ifndef HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
#define HGL_GRAPH_VULKAN_MATERIAL_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class Shader;
class DescriptorSetLayoutCreater;
class MaterialInstance;
class VertexAttributeBinding;

/**
 * 材质类<br>
 * 用于管理shader，提供DescriptorSetLayoutCreater
 */
class Material
{
    Shader *shader;
    DescriptorSetLayoutCreater *dsl_creater;

public:

    Material(Shader *s,DescriptorSetLayoutCreater *dslc)
    {
        shader=s;
        dsl_creater=dslc;
    }
    ~Material();

    MaterialInstance *CreateInstance();
};//class Material

/**
 * 材质实例<br>
 * 用于在指定Material的情况下，具体绑定UBO/TEXTURE等，提供pipeline
 */
class MaterialInstance
{
    const Material *mat;                                                                            ///<这里的是对material的完全引用，不做任何修改
    VertexAttributeBinding *vab;                                                         ///<这里的vis是Material中vis的复制体

public:

    MaterialInstance(Material *m,VertexAttributeBinding *vi)
    {
        mat=m;
        vab=vi;
    }

    ~MaterialInstance();
};//class MaterialInstance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MATERIAL_INCLUDE
