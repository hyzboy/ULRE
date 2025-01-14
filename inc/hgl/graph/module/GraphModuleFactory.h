#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/RenderFramework.h>

VK_NAMESPACE_BEGIN

class GraphModuleFactory
{
public:

    GraphModuleFactory()=default;
    virtual ~GraphModuleFactory()=default;

    virtual GraphModule *Create(RenderFramework *)=0;
};//class GraphModuleFactory

bool RegistryGraphModuleFactory(const char *module_name,GraphModuleFactory *);

template<typename T> class RegistryGraphModule:public GraphModuleFactory
{
public:

    GraphModule *Create(RenderFramework *rf) override
    {
        if(!rf)
            return(nullptr);

        Map<AIDName,GraphModule *> dgm_map;

        //检查依赖模块
        {
            const auto &dependent_modules=T::GetDependentModules();

            if(!dependent_modules.IsEmpty())
            {
                for(const AIDName &name:dependent_modules)
                {
                    GraphModule *dgm=rf->GetModule(name,true);

                    if(!dgm)
                        return(nullptr);

                    dgm_map.Add(name,dgm);
                }
            }
        }

        T *gm=T::CreateModule(rf,dgm_map);

        return(gm);
    }
};//template<typename T> class RegistryGraphModule:public GraphModuleFactory

#define REGISTRY_GRAPH_MODULE(Class)    {RegistryGraphModuleFactory(#Class,new RegistryGraphModule<Class>);}

VK_NAMESPACE_END
