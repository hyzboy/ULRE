#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>

STD_MTL_NAMESPACE_BEGIN

class MaterialFactory
{
public:

    virtual AnsiString GetName()const=0;

    //virtual const bool GetMaterialName()const=0;

    //virtual const PrimitiveType supportPrimitive()const=0;

    //virtual const bool is2D()const=0;
    //virtual const bool is3D()const=0;

    //virtual const bool hasCamera()const=0;
    //virtual const bool hasLocalToWorld()const=0;

    //virtual const CoordinateSystem2D get2DCoordinateSystem()const=0;

    virtual MaterialCreateInfo *Create(MaterialCreateConfig *);

};//class MaterialFactory

bool                RegistryMaterialFactory(MaterialFactory *);
MaterialFactory *   GetMaterialFactory(const AnsiString &);

template<typename T> class RegistryMaterialFactoryClass
{
public:

    RegistryMaterialFactoryClass()
    {
        STD_MTL_NAMESPACE::RegistryMaterialFactory(new T);
    }
};

#define DEFINE_MATERIAL_FACTORY(name) namespace{static RegistryMaterialFactoryClass<MaterialFactory##name> MaterialFactoryInstance_##name;}

MaterialCreateInfo *CreateMaterialCreateInfo(const AnsiString &,MaterialCreateConfig *cfg=nullptr,const VILConfig *vil_cfg=nullptr);

STD_MTL_NAMESPACE_END
