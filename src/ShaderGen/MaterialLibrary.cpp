#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<hgl/type/UnorderedMap.h>

#include<memory>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=UnorderedMap<int,std::unique_ptr<MaterialFactory>>;

    MaterialFactoryMap &GetMaterialFactoryMap()
    {
        static MaterialFactoryMap material_factory_map;
        return material_factory_map;
    }
}//namespace

bool RegisterMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const MaterialName &name=mf->GetName();
    const int name_id=name.GetID();

    auto &material_factory_map=GetMaterialFactoryMap();

    if(material_factory_map.ContainsKey(name_id))
    {
        delete mf;
        return(false);
    }

    material_factory_map.Add(name_id,std::unique_ptr<MaterialFactory>(mf));

    return(true);
}

MaterialFactory *GetMaterialFactory(const MaterialName &name)
{
    auto &material_factory_map=GetMaterialFactoryMap();
    std::unique_ptr<MaterialFactory>* ptr = material_factory_map.GetValuePointer(name.GetID());
    return ptr ? ptr->get() : nullptr;
}

void ClearMaterialFactory()
{
    GetMaterialFactoryMap().Clear();
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const MaterialName &name,MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    MaterialFactory *mf=GetMaterialFactory(name);

    if(!mf)
        return(nullptr);

    return mf->Create(dev_attr,cfg);
}

STD_MTL_NAMESPACE_END
