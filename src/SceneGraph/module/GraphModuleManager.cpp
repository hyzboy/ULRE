#include<hgl/graph/module/GraphModule.h>
#include<hgl/log/Logger.h>
#include <sstream>
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
    do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] ~GraphModuleManager() - Start, module_list.size()=" << module_list.GetCount(); GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
    // 阶段1: 反向调用所有模块的Release()方法，让它们清理资源
    // 释放顺序应与创建顺序相反，避免依赖先被销毁
    do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] ~GraphModuleManager() - Phase 1: Calling Release() on all modules"; GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
    {
        GraphModule **gm = module_list.last();
        GraphModule **begin = module_list.begin();

        while (gm >= begin)
        {
            if (*gm)
            {
                do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] Calling Release() on module: " << (*gm)->GetName().c_str(); GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
                (*gm)->Release();
                do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] Release() complete for: " << (*gm)->GetName().c_str(); GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
            }
            --gm;
        }
    }
    do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] ~GraphModuleManager() - Phase 1 complete"; GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
    // 阶段2: 删除所有模块
    // 此时GPU资源已经被Release()清理过了
    do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] ~GraphModuleManager() - Phase 2: Deleting all modules"; GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
    {
        GraphModule **gm = module_list.begin();
        GraphModule **end = module_list.end();

        while (gm < end)
        {
            if (*gm)
            {
                do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] Deleting module: " << (*gm)->GetName().c_str(); GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
                delete *gm;
            }
            ++gm;
        }
    }

    module_list.Clear();
    do { std::ostringstream _ulre_log_oss; _ulre_log_oss << "[DEBUG] ~GraphModuleManager() - Complete"; GLogInfo("%s", _ulre_log_oss.str().c_str()); } while(0);
}

}//namespace hgl::graph
