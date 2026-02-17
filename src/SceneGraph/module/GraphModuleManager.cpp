#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/core/GraphicsContext.h>

namespace hgl::graph{

VulkanDevice *GraphModuleManager::GetDevice()const
{
    return graphics_context?graphics_context->GetDevice():nullptr;
}

bool GraphModuleManager::Register(GraphModule *gm)
{
    if(!gm)
        return(false);

    const size_t type_hash=gm->GetTypeHash();

    if(module_map.ContainsKey(type_hash))
        return(false);

    module_list.Add(gm);
    module_map.Add(type_hash,gm);

    if(graphics_context)
        gm->SetGraphicsContext(graphics_context);

    return(true);
}

void GraphModuleManager::SetGraphicsContext(GraphicsContext *gc)
{
    graphics_context=gc;

    if(module_list.GetCount()==0)
        return;

    for(auto **gm=module_list.begin();gm<=module_list.last();++gm)
    {
        if(*gm)
            (*gm)->SetGraphicsContext(gc);
    }
}

bool GraphModuleManager::Unregister(GraphModule *gm)
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
    std::cout << "[DEBUG] ~GraphModuleManager() - Start, module_list.size()=" << module_list.GetCount() << std::endl;
    // 阶段1: 反向调用所有模块的Release()方法，让它们清理资源
    // 释放顺序应与创建顺序相反，避免依赖先被销毁
    std::cout << "[DEBUG] ~GraphModuleManager() - Phase 1: Calling Release() on all modules" << std::endl;
    {
        GraphModule **gm = module_list.last();
        GraphModule **begin = module_list.begin();

        while (gm >= begin)
        {
            if (*gm)
            {
                std::cout << "[DEBUG] Calling Release() on module: " << (*gm)->GetName().c_str() << std::endl;
                (*gm)->Release();
                std::cout << "[DEBUG] Release() complete for: " << (*gm)->GetName().c_str() << std::endl;
            }
            --gm;
        }
    }
    std::cout << "[DEBUG] ~GraphModuleManager() - Phase 1 complete" << std::endl;

    // 阶段2: 删除所有模块
    // 此时GPU资源已经被Release()清理过了
    std::cout << "[DEBUG] ~GraphModuleManager() - Phase 2: Deleting all modules" << std::endl;
    {
        GraphModule **gm = module_list.begin();
        GraphModule **end = module_list.end();

        while (gm < end)
        {
            if (*gm)
            {
                std::cout << "[DEBUG] Deleting module: " << (*gm)->GetName().c_str() << std::endl;
                delete *gm;
            }
            ++gm;
        }
    }

    module_list.Clear();
    std::cout << "[DEBUG] ~GraphModuleManager() - Complete" << std::endl;
}

}//namespace hgl::graph
