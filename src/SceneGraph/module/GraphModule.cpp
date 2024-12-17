#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

bool GraphModulesMap::Add(GraphModule *gm)
{
    if(!gm)return(false);

    if(gm_map.ContainsKey(gm->GetName()))
        return(false);

    gm_list.Add(gm);
    gm_map.Add(gm->GetName(),gm);
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

GraphModule *GraphModule::GetDependentModule(const AnsiIDName &name)
{
    GraphModule *dm;

    if(dependent_modules.Get(name,dm))
        return(dm);

    return(nullptr);
}

bool GraphModule::Init(GraphModulesMap *gmm)
{
    auto dm_list=GetDependentModules();
    
    if(!dm_list.IsEmpty())
    {
        if(!gmm)
            return(false);

        if(!gmm->IsEmpty())
        {
            for(auto dm_name:dm_list)
            {
                GraphModule *dm;
                
                if(gmm->Get(dm_name,dm))
                {
                    if(dm->IsInited())
                    {
                        dependent_modules.Add(dm_name,dm);
                        continue;
                    }
                }

                return(false);
            }
        }
    }

    module_inited=true;
    return(true);
}

VK_NAMESPACE_END
