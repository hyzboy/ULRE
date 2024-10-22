#include<hgl/graph/module/GraphModuleFactory.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>

VK_NAMESPACE_BEGIN

namespace
{
    using GraphModuleFactoryMap=ObjectMap<AnsiString,GraphModuleFactory>;

    static GraphModuleFactoryMap *gmf_map=nullptr;
}

void InitGraphModuleFactory()
{
    if(gmf_map)
        return;

    gmf_map=new GraphModuleFactoryMap;
}

void ClearGraphModuleFactory()
{
    SAFE_CLEAR(gmf_map);
}

bool RegistryGraphModuleFactory(const char *module_name,GraphModuleFactory *gmf)
{
    if(!module_name||!gmf)
        return(false);

    if(!gmf_map)
        InitGraphModuleFactory();

    AnsiString name=module_name;

    if(gmf_map->ContainsKey(name))
        return(false);

    gmf_map->Add(name,gmf);

    return(true);
}

GraphModule *CreateGraphModule(const AnsiString &name,GraphModuleManager *gmm)
{
    if(!gmm)
        return(nullptr);

    if(name.IsEmpty())
        return(nullptr);

    if(!gmf_map)
        return(nullptr);

    GraphModuleFactory *gmf;

    if(!gmf_map->Get(name,gmf))
        return(nullptr);

    return gmf->Create(gmm);
}

VK_NAMESPACE_END
