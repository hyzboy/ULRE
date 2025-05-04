#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/IDName.h>
#include<hgl/type/String.h>

STD_MTL_NAMESPACE_BEGIN

enum class MaterialDomain
{
    UI,                                 ///<用户界面,一般2D均使用这种
    
    Gizmo,                              ///<Gizmo材质(基本同Surface，但很多数值和处理逻辑会写死)

    Surface,                            ///<常规的3D表面材质
    DeferredDecal,                      ///<延迟贴花材质

    PostProcess,                        ///<后期处理材质

    ENUM_CLASS_RANGE(UI,PostProcess)
};

class MaterialFactory
{
public:

    virtual AIDName GetName()const=0;

    //virtual const bool GetMaterialName()const=0;

    virtual const MaterialDomain GetDomain()const=0;

    //virtual const PrimitiveType supportPrimitive()const=0;

    //virtual const bool is2D()const=0;
    //virtual const bool is3D()const=0;

    //virtual const bool hasCamera()const=0;
    //virtual const bool hasLocalToWorld()const=0;

    //virtual const CoordinateSystem2D get2DCoordinateSystem()const=0;

    virtual MaterialCreateInfo *Create();

};//class MaterialFactory

bool RegistryMaterialFactory(MaterialFactory *);
MaterialFactory *GetMaterialFactory(const AIDName &);
void ClearMaterialFactory();

template<typename T> class RegistryMaterialFactoryClass
{
public:

    RegistryMaterialFactoryClass()
    {
        STD_MTL_NAMESPACE::RegistryMaterialFactory(new T);
    }
};

#define DEFINE_MATERIAL_FACTORY(name) namespace{static RegistryMaterialFactoryClass<MaterialFactory##name> MaterialFactoryInstance_##name;}

struct Material2DCreateConfig;
struct Material3DCreateConfig;

Material *CreateMaterial2D(const AnsiString &,Material2DCreateConfig *cfg=nullptr);
Material *CreateMaterial3D(const AnsiString &,Material3DCreateConfig *cfg=nullptr);

STD_MTL_NAMESPACE_END
