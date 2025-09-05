#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/type/IDName.h>

STD_MTL_NAMESPACE_BEGIN

HGL_DEFINE_ANSI_IDNAME(MaterialName)

class MaterialFactory
{
public:

    virtual const MaterialName &GetName()const=0;

    //virtual const bool GetMaterialName()const=0;

    //virtual const PrimitiveType supportPrimitive()const=0;

    //virtual const bool is2D()const=0;
    //virtual const bool is3D()const=0;

    //virtual const bool hasCamera()const=0;
    //virtual const bool hasLocalToWorld()const=0;

    //virtual const CoordinateSystem2D get2DCoordinateSystem()const=0;

    virtual MaterialCreateInfo *Create(const VulkanDevAttr *dev_attr,MaterialCreateConfig *)=0;

};//class MaterialFactory

bool                RegisterMaterialFactory(MaterialFactory *);
MaterialFactory *   GetMaterialFactory(const MaterialName &);

template<typename T> class RegisterMaterialFactoryClass
{
public:

    RegisterMaterialFactoryClass()
    {
        STD_MTL_NAMESPACE::RegisterMaterialFactory(new T);
    }
};//class RegisterMaterialFactoryClass

#define DEFINE_MATERIAL_FACTORY_CLASS(name,cfg_type) \
namespace inline_material   \
{   \
    constexpr const char name[]=#name; \
}   \
\
MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr,cfg_type *); \
\
inline MaterialCreateInfo *Create##name(const VulkanDevAttr *dev_attr)  \
{   \
    cfg_type cfg;   \
    return Create##name(dev_attr,&cfg);  \
}   \
\
namespace \
{   \
    class MaterialFactory##name:public MaterialFactory  \
    {   \
    public: \
    \
        const MaterialName &GetName()const override \
        {   \
            static MaterialName mtl_name(#name);    \
            return mtl_name;    \
        }   \
    \
        MaterialCreateInfo *Create(const VulkanDevAttr *dev_attr,MaterialCreateConfig *cfg) override  \
        {   \
            return Create##name(dev_attr,(cfg_type *)cfg);    \
        }   \
    };  \
    \
    static RegisterMaterialFactoryClass<MaterialFactory##name> MaterialFactoryInstance_##name;   \
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const MaterialName &,MaterialCreateConfig *cfg);

inline MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const char *mtl_name,MaterialCreateConfig *cfg)
{
    MaterialName mtl_id_name(mtl_name);

    return CreateMaterialCreateInfo(dev_attr,mtl_id_name,cfg);
}

inline MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const AnsiString &mtl_name,MaterialCreateConfig *cfg)
{
    MaterialName mtl_id_name(mtl_name);

    return CreateMaterialCreateInfo(dev_attr,mtl_id_name,cfg);
}

STD_MTL_NAMESPACE_END

