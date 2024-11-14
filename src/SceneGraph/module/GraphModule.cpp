#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

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

VK_NAMESPACE_END
