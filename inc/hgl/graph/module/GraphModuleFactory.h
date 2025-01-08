#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class GraphModuleFactory
{
public:

    GraphModuleFactory()=default;
    virtual ~GraphModuleFactory()=default;

    virtual GraphModule *Create(GraphModuleManager *)=0;
};//class GraphModuleFactory

bool RegistryGraphModuleFactory(const char *module_name,GraphModuleFactory *);

template<typename T> class RegistryGraphModule:public GraphModuleFactory
{
public:

    GraphModule *Create(GraphModuleManager *gmm) override
    {
        if(!gmm)
            return(nullptr);

        Map<AnsiIDName,GraphModule *> dgm_map;

        //检查依赖模块
        {
            const auto &dependent_modules=T::GetDependentModules();

            if(!dependent_modules.IsEmpty())
            {
                for(const AnsiIDName &name:dependent_modules)
                {
                    GraphModule *dgm=gmm->GetModule(name,true);

                    if(!dgm)
                        return(nullptr);

                    dgm_map.Add(name,dgm);
                }
            }
        }

        GraphModule *gm=new T(gmm);

        if(!gm->Init())
        {
            delete gm;
            return(nullptr);
        }

        return(gm);
    }
};//template<typename T> class RegistryGraphModule:public GraphModuleFactory

#define REGISTRY_GRAPH_MODULE(Class)    {RegistryGraphModuleFactory(#Class,new RegistryGraphModule<Class>);}

VK_NAMESPACE_END
