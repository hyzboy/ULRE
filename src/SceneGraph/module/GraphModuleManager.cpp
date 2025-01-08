#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

void InitGraphModuleFactory();
void ClearGraphModuleFactory();

namespace
{
    using GraphModuleManagerMap=Map<RenderFramework *,GraphModuleManager *>;

    static GraphModuleManagerMap *graph_module_manager_map=nullptr;
}

GraphModuleManager *InitGraphModuleManager(RenderFramework *rf)
{
    if(!rf)
        return(nullptr);

    if(graph_module_manager_map)
    {
        if(graph_module_manager_map->ContainsKey(rf))
            return(nullptr);
    }
    else
    {
        InitGraphModuleFactory();
        RegistryCommonGraphModule();

        graph_module_manager_map=new GraphModuleManagerMap;
    }

    GraphModuleManager *gmm=new GraphModuleManager(rf);

    graph_module_manager_map->Add(rf,gmm);

    return gmm;
}

bool ClearGraphModuleManager(RenderFramework *rf)
{
    if(!rf)
        return(false);

    if(!graph_module_manager_map)
        return(false);

    GraphModuleManager *gmm;
    
    if(!graph_module_manager_map->Get(rf,gmm))
        return(false);

    graph_module_manager_map->DeleteByKey(rf);

    delete gmm;

    if(graph_module_manager_map->IsEmpty())
    {
        ClearGraphModuleFactory();
        SAFE_CLEAR(graph_module_manager_map);
    }

    return(true);
}

GraphModuleManager *GetGraphModuleManager(RenderFramework *rf)
{
    if(!rf)
        return(nullptr);

    if(!graph_module_manager_map)
        return(nullptr);

    GraphModuleManager *gmm;

    if(graph_module_manager_map->Get(rf,gmm))
        return gmm;

    return(nullptr);
}

GraphModule *CreateGraphModule(const AnsiIDName &name,GraphModuleManager *gmm);

GraphModule *GraphModuleManager::GetModule(const AnsiIDName &name,bool create)
{
    GraphModule *gm=graph_module_map.Get(name);

    if(gm)
        return gm;

    if(create)
    {
        gm=CreateGraphModule(name,this);

        if(gm)
            graph_module_map.Add(gm);
    }

    return gm;
}

GraphModuleManager::GraphModuleManager(RenderFramework *rf)
{
    framework=rf;
    device=rf->GetDevice();
}

GraphModuleManager::~GraphModuleManager()
{
    graph_module_map.Destory();
}

void GraphModuleManager::ReleaseModule(GraphModule *gm)
{
    if(!gm)
        return;

    graph_module_map.Release(gm);
}

void GraphModuleManager::OnResize(const VkExtent2D &extent)
{
    if(graph_module_map.IsEmpty())
        return;

    for(auto *gm:graph_module_map.gm_list)
    {
        gm->OnResize(extent);
    }
}

VK_NAMESPACE_END
