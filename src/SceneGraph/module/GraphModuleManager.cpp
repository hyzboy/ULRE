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
    GraphModule *gm;

    if(graph_module_map.Get(name,gm))
        return gm;

    if(create)
    {
        gm=CreateGraphModule(name,this);

        if(gm)
        {
            graph_module_map.Add(name,gm);
            module_list.Add(gm);
        }

        return gm;
    }

    return nullptr;
}

GraphModuleManager::GraphModuleManager(RenderFramework *rf)
{
    framework=rf;
    device=rf->GetDevice();
}

GraphModuleManager::~GraphModuleManager()
{
    //按顺序加入module_list的，要倒着释放

    GraphModule **gm=module_list.end();

    while(gm>module_list.begin())
    {
        --gm;
        delete *gm;
    }

    //其实下面释不释放都无所谓了
    module_list.Clear();
    graph_module_map.Clear();
}

void GraphModuleManager::ReleaseModule(GraphModule *gm)
{
    if(!gm)
        return;

    graph_module_map.DeleteByValue(gm);
    delete gm;
}

void GraphModuleManager::OnResize(const VkExtent2D &extent)
{
    if(graph_module_map.IsEmpty())
        return;

    for(auto *gm:graph_module_map)
    {
        gm->value->OnResize(extent);
    }
}

VK_NAMESPACE_END
