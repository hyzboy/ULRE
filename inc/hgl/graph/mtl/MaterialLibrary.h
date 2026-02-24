#pragma once

#include<hgl/vk/VK.h>
#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/type/IDName.h>

namespace hgl::graph::mtl{

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
        hgl::graph::mtl::RegisterMaterialFactory(new T);
    }
};//class RegisterMaterialFactoryClass

/// 仅声明材质创建函数与 inline_material 名称常量，不产生工厂注册代码。
/// 用于配置头文件，使配置头文件无需引入工厂注册的副作用。
#define DECLARE_MATERIAL_CREATOR(name,cfg_type) \
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
}

/// 仅定义工厂类并完成自动注册，不重复声明创建函数。
/// 需在对应的 DECLARE_MATERIAL_CREATOR 已展开之后才能使用（函数须先声明）。
/// 用于专属的工厂注册头文件（MaterialFactory2D.h / MaterialFactory3D.h）。
#define IMPL_MATERIAL_FACTORY(name,cfg_type) \
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

/// 一次性完成函数声明 + 工厂注册，仅供需要在单个文件中同时完成两者时使用。
#define DEFINE_MATERIAL_FACTORY_CLASS(name,cfg_type) \
DECLARE_MATERIAL_CREATOR(name,cfg_type) \
IMPL_MATERIAL_FACTORY(name,cfg_type)

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

}//namespace hgl::graph::mtl

