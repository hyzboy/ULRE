#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<hgl/type/Map.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=ObjectMap<int,MaterialFactory>;
    using MaterialFactoryMapPtr=MaterialFactoryMap *;

    MaterialFactoryMapPtr material_factory_map=nullptr;
}//namespace

bool RegistryMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const MaterialName &name=mf->GetName();
    const int name_id=name.GetID();

    if(!material_factory_map)
    {
        material_factory_map=new MaterialFactoryMap();
    }

    if(material_factory_map->ContainsKey(name_id))
        return(false);

    material_factory_map->Add(name_id,mf);

    return(true);
}

MaterialFactory *GetMaterialFactory(const MaterialName &name)
{
    if(!material_factory_map)
        return(nullptr);

    return (*material_factory_map)[name.GetID()];
}

void ClearMaterialFactory()
{
    SAFE_CLEAR(material_factory_map);
}

MaterialCreateInfo *CreateMaterialCreateInfo(const MaterialName &name,MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    MaterialFactory *mf=GetMaterialFactory(name);

    if(!mf)
        return(nullptr);

    return mf->Create(cfg);
}

STD_MTL_NAMESPACE_END
