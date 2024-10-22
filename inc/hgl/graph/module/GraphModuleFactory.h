#pragma once

#include<hgl/graph/VKNamespace.h>

VK_NAMESPACE_BEGIN

class GraphModule;
class GraphModuleManager;

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
    bool registry_success;

public:

    RegistryGraphModule()
    {
        registry_success=RegistryGraphModuleFactory(T::GetModuleName(),this);
    }

    GraphModule *Create(GraphModuleManager *gmm) override
    {
        if(!registry_success)
            return(nullptr);

        if(!gmm)
            return(nullptr);

        return(new T(gmm));
    }
};//template<typename T> class RegistryGraphModule:public GraphModuleFactory

#define REGISTRY_GRAPH_MODULE(Class)    namespace{static RegistryGraphModule<Class> registry_##Class;}

VK_NAMESPACE_END