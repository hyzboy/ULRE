#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

void InitGraphModuleFactory();
void ClearGraphModuleFactory();

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
        InitGraphModuleFactory();
        RegistryCommonGraphModule();

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

    if(graph_module_manager_map->IsEmpty())
    {
        ClearGraphModuleFactory();
        SAFE_CLEAR(graph_module_manager_map);
    }

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
