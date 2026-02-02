#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/type/IDName.h>
#include<hgl/type/UnorderedMap.h>

STD_MTL_NAMESPACE_BEGIN

namespace
{
    using MaterialFactoryMap=UnorderedMap<int,MaterialFactory *>;

    MaterialFactoryMap material_factory_map;
}//namespace

bool RegisterMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const MaterialName &name=mf->GetName();
    const int name_id=name.GetID();

    if(material_factory_map.ContainsKey(name_id))
        return(false);

    material_factory_map.Add(name_id,mf);

    return(true);
}

MaterialFactory *GetMaterialFactory(const MaterialName &name)
{
    MaterialFactory** ptr = material_factory_map.GetValuePointer(name.GetID());
    return ptr ? *ptr : nullptr;
}

void ClearMaterialFactory()
{
    material_factory_map.Clear();
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
