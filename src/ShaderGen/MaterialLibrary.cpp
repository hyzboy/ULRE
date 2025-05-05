#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<tsl/robin_map.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=tsl::robin_map<AnsiString,MaterialFactory *>;
    using MaterialFactoryMapPtr=MaterialFactoryMap *;

    MaterialFactoryMapPtr material_factory_map=nullptr;
}//namespace

bool RegistryMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const AnsiString &name=mf->GetName();

    if(!material_factory_map)
    {
        material_factory_map=new MaterialFactoryMap;
    }

    if(material_factory_map->contains(name))
        return(false);

    material_factory_map->insert({name,mf});

    return(true);
}

MaterialFactory *GetMaterialFactory(const AnsiString &name)
{
    if(!material_factory_map)
        return(nullptr);

    auto it=material_factory_map->find(name);

    if(it==material_factory_map->end())
        return(nullptr);

    return(it->second);
}

void ClearMaterialFactory()
{
    if(!material_factory_map)
        return;

    SAFE_CLEAR_STD_MAP(*material_factory_map)

    delete material_factory_map;
    material_factory_map=nullptr;
}

MaterialCreateInfo *CreateMaterialCreateInfo(const AnsiString &name,MaterialCreateConfig *cfg,const VILConfig *vil_cfg)
{
    if(name.IsEmpty())
        return(nullptr);

    MaterialFactory *mf=GetMaterialFactory(name);

    if(!mf)
        return(nullptr);

    return mf->Create(cfg);
}

STD_MTL_NAMESPACE_END
