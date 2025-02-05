#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/RenderFramework.h>

VK_NAMESPACE_BEGIN

GPUDevice *GraphModuleManager::GetDevice()const
{
    return render_framework->GetDevice();
}

bool GraphModuleManager::Registry(GraphModule *gm)
{
    if(!gm)
        return(false);

    const size_t type_hash=gm->GetTypeHash();

    if(module_map.ContainsKey(type_hash))
        return(false);

    module_list.Add(gm);
    module_map.Add(type_hash,gm);

    return(true);
}

bool GraphModuleManager::Unregistry(GraphModule *gm)
{
    if(!gm)
        return(false);

    const size_t type_hash=gm->GetTypeHash();

    if(!module_map.ContainsKey(type_hash))
        return(false);

    if(module_list.DeleteByValue(gm)<0)
        return(false);

    delete gm;
    return(true);
}

GraphModuleManager::~GraphModuleManager()
{
    GraphModule **gm=module_list.last();
    GraphModule **begin=module_list.begin();

    while(gm>=begin)
    {
        delete *gm;
        --gm;
    }

    module_list.Clear();
}

VK_NAMESPACE_END
