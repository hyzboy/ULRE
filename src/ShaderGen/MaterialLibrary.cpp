#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<hgl/type/Map.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=ObjectMap<AnsiString,MaterialFactory>;
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
        material_factory_map=new MaterialFactoryMap();
    }

    if(material_factory_map->ContainsKey(name))
        return(false);

    material_factory_map->Add(name,mf);

    return(true);
}

MaterialFactory *GetMaterialFactory(const AnsiString &name)
{
    if(!material_factory_map)
        return(nullptr);

    return (*material_factory_map)[name];
}

void ClearMaterialFactory()
{
    SAFE_CLEAR(material_factory_map);
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
