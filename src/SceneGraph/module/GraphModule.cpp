#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

bool GraphModulesMap::Add(GraphModule *gm)
{
    if(!gm)
        return(false);

    if(gm_set.Contains(gm))
        return(false);

    gm_list.Add(gm);
    gm_set.Add(gm);
    gm_map_by_name.Add(gm->GetName(),gm);
    gm_map_by_hash.Add(gm->GetTypeHash(),gm);
}

void GraphModulesMap::Destory()
{
//按顺序加入module_list的，要倒着释放

    auto *m=gm_list.last();
    auto *begin=gm_list.begin();

    while(m>=begin)
    {
        delete m;
        --m;
    }

    gm_list.Clear();
    gm_set.Clear();
    gm_map_by_name.Clear();
    gm_map_by_hash.Clear();
}

bool GraphModulesMap::Release(GraphModule *gm)
{
    if(!gm)
        return(false);
    if(!gm_set.Contains(gm))
        return(false);

    //因为总计就没几个数据，所以无序关心效能

    gm_set.Delete(gm);
    gm_list.DeleteByValue(gm);
    gm_map_by_name.DeleteByValue(gm);
    gm_map_by_hash.DeleteByValue(gm);

    return(true);
}

GraphModule::GraphModule(RenderFramework *rf,const AnsiIDName &name)
{
    render_framework=rf;
    module_name=name;

    LOG_INFO("GraphModule::GraphModule: "+AnsiString(module_name.GetName()))
}

GraphModule::~GraphModule()
{
    LOG_INFO("GraphModule::~GraphModule: "+AnsiString(module_name.GetName()))
}

GraphModule *GraphModule::GetModule(const AnsiIDName &name,bool create)
{
    render_framework->GetModule(name,create);
}

VK_NAMESPACE_END
