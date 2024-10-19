#include<hgl/graph/RenderModule.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

namespace
{
    using RenderModuleMap=Map<OSString,RenderModuleCreateFunc>;

    RenderModuleMap render_module_map;
}//namespace

bool RegistryRenderModule(const OSString &name,RenderModuleCreateFunc func)
{
    if(name.IsEmpty()||!func)
        return(false);

    if(render_module_map.ContainsKey(name))
        return(false);

    render_module_map.Add(name,func);

    return(true);
}

RenderModule *GetRenderModule(const OSString &name)
{
    if(name.IsEmpty())
        return(nullptr);

    RenderModuleCreateFunc func;

    if(!render_module_map.Get(name,func))
        return(nullptr);

    return func();
}

VK_NAMESPACE_END
