#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

namespace
{
    using GraphModuleManagerMap=Map<GPUDevice *,GraphModuleManager *>;

    static GraphModuleManagerMap *graph_module_manager_map=nullptr;
}

GraphModuleManager *InitGraphModuleManager(GPUDevice *dev)
{
    if(!dev)
        return(nullptr);

    if(graph_module_manager_map)
    {
        if(graph_module_manager_map->ContainsKey(dev))
            return(nullptr);
    }
    else
    {
        graph_module_manager_map=new GraphModuleManagerMap;
    }

    GraphModuleManager *gmm=new GraphModuleManager(dev);

    graph_module_manager_map->Add(dev,gmm);

    return gmm;
}

bool ClearGraphModuleManager(GPUDevice *dev)
{
    if(!dev)
        return(false);

    if(!graph_module_manager_map)
        return(false);

    GraphModuleManager *gmm;
    
    if(!graph_module_manager_map->Get(dev,gmm))
        return(false);

    graph_module_manager_map->DeleteByKey(dev);

    delete gmm;
    return(true);
}

GraphModuleManager *GetGraphModuleManager(GPUDevice *dev)
{
    if(!dev)
        return(nullptr);

    if(!graph_module_manager_map)
        return(nullptr);

    GraphModuleManager *gmm;

    if(graph_module_manager_map->Get(dev,gmm))
        return gmm;

    return(nullptr);
}

bool GraphModuleManager::Registry(const AnsiString &name,GraphModule *gm)
{
    if(!gm)
        return(false);
    if(name.IsEmpty())
        return(false);

    if(graph_module_map.ContainsKey(name))
        return(false);

    graph_module_map.Add(name,gm);
    return(true);
}

GraphModule *GraphModuleManager::GetModule(const AnsiString &name)
{
    GraphModule *gm;

    if(graph_module_map.Get(name,gm))
        return gm;

    return nullptr;
}

GraphModuleManager::~GraphModuleManager()
{
    for(auto *gm:graph_module_map)
    {
        delete gm->value;
    }
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
