#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

namespace
{
    using GraphModuleMap=Map<AnsiString,GraphModuleCreateFunc>;

    GraphModuleMap render_module_map;
}//namespace

bool RegistryGraphModule(const AnsiString &name,GraphModuleCreateFunc func)
{
    if(name.IsEmpty()||!func)
        return(false);

    if(render_module_map.ContainsKey(name))
        return(false);

    render_module_map.Add(name,func);

    return(true);
}

GraphModule *GetGraphModule(const AnsiString &name)
{
    if(name.IsEmpty())
        return(nullptr);

    GraphModuleCreateFunc func;

    if(!render_module_map.Get(name,func))
        return(nullptr);

    return func();
}

VK_NAMESPACE_END
