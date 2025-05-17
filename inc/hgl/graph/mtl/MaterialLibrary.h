#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/type/IDName.h>

STD_MTL_NAMESPACE_BEGIN

DefineIDName(MaterialName,   char)

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

    virtual MaterialCreateInfo *Create(const GPUDeviceAttribute *dev_attr,MaterialCreateConfig *)=0;

};//class MaterialFactory

bool                RegistryMaterialFactory(MaterialFactory *);
MaterialFactory *   GetMaterialFactory(const MaterialName &);

template<typename T> class RegistryMaterialFactoryClass
{
public:

    RegistryMaterialFactoryClass()
    {
        STD_MTL_NAMESPACE::RegistryMaterialFactory(new T);
    }
};//class RegistryMaterialFactoryClass

#define DEFINE_MATERIAL_FACTORY_CLASS(name,create_func,cfg_type) \
MaterialCreateInfo *Create##name(const GPUDeviceAttribute *dev_attr,cfg_type *); \
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
        MaterialCreateInfo *Create(const GPUDeviceAttribute *dev_attr,MaterialCreateConfig *cfg) override  \
        {   \
            return create_func(dev_attr,(cfg_type *)cfg);    \
        }   \
    };  \
    \
    static RegistryMaterialFactoryClass<MaterialFactory##name> MaterialFactoryInstance_##name;   \
}

MaterialCreateInfo *CreateMaterialCreateInfo(const GPUDeviceAttribute *dev_attr,const MaterialName &,MaterialCreateConfig *cfg);

inline MaterialCreateInfo *CreateMaterialCreateInfo(const GPUDeviceAttribute *dev_attr,const char *mtl_name,MaterialCreateConfig *cfg)
{
    MaterialName mtl_id_name(mtl_name);

    return CreateMaterialCreateInfo(dev_attr,mtl_id_name,cfg);
}

inline MaterialCreateInfo *CreateMaterialCreateInfo(const GPUDeviceAttribute *dev_attr,const AnsiString &mtl_name,MaterialCreateConfig *cfg)
{
    MaterialName mtl_id_name(mtl_name);

    return CreateMaterialCreateInfo(dev_attr,mtl_id_name,cfg);
}

STD_MTL_NAMESPACE_END

