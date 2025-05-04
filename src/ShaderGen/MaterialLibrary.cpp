#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<tsl/robin_map.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=tsl::robin_map<AIDName,MaterialFactory *>;

    MaterialFactoryMap *material_factory_map=nullptr;
}//namespace

bool RegistryMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const AIDName name=mf->GetName();

    if(!material_factory_map)
    {
        material_factory_map=new MaterialFactoryMap;
    }

    if(material_factory_map->contains(name))
        return(false);

    material_factory_map->insert({name,mf});

    const MaterialDomain &domain=mf->GetDomain();



    return(true);
}

MaterialFactory *GetMaterialFactory(const AIDName &name)
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

Material *CreateMaterial2D(const AnsiString &name,Material2DCreateConfig *cfg)
{

}

STD_MTL_NAMESPACE_END
