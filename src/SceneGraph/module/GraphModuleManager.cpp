#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/core/GraphicsContext.h>

VK_NAMESPACE_BEGIN

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
    // 阶段1: 反向调用所有模块的Release()方法，让它们清理资源
    // 释放顺序应与创建顺序相反，避免依赖先被销毁
    {
        GraphModule **gm = module_list.last();
        GraphModule **begin = module_list.begin();

        while (gm >= begin)
        {
            if (*gm)
            {
                (*gm)->Release();
            }
            --gm;
        }
    }

    // 阶段2: 删除所有模块
    // 此时GPU资源已经被Release()清理过了
    {
        GraphModule **gm = module_list.last();
        GraphModule **begin = module_list.begin();

        while (gm >= begin)
        {
            delete *gm;
            --gm;
        }
    }

    module_list.Clear();
}

VK_NAMESPACE_END
