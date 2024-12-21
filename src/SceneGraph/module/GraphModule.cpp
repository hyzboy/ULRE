#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

bool GraphModulesMap::Add(GraphModule *gm)
{
    if(!gm)
        return(false);

    if(gm_set.Contains(gm))
        return(false);

    
    gm_set.Add(gm);
    gm_map_by_name.Add(gm->GetName(),gm);

}

GraphModule::GraphModule(GraphModuleManager *gmm,const AnsiIDName &name)
{
    module_manager=gmm;
    module_name=name;

    LOG_INFO("GraphModule::GraphModule: "+AnsiString(module_name.GetName()))
}

GraphModule::~GraphModule()
{
    LOG_INFO("GraphModule::~GraphModule: "+AnsiString(module_name.GetName()))
}

bool GraphModule::InitDependentModules(GraphModulesMap *)
{
    auto dm_list=GetDependentModules();
    
    if(!dm_list.IsEmpty())
    {
        if(!gmm)
            return(false);

        if(gmm->IsEmpty())
            return(false);

        for(auto dm_name:dm_list)
        {
            GraphModule *dm=gmm->Get(dm_name);

            if(dm&&dm->IsInited())
            {
                dependent_modules.Add(dm);
                continue;
            }

            return(false);
        }
    }

    return(true);
}

VK_NAMESPACE_END
