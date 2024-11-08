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
public:

    GraphModule *Create(GraphModuleManager *gmm) override
    {
        if(!gmm)
            return(nullptr);

        GraphModule *gm=new T(gmm);

        if(!gm->Init())
        {
            delete gm;
            return(nullptr);
        }

        return(true);
    }
};//template<typename T> class RegistryGraphModule:public GraphModuleFactory

#define REGISTRY_GRAPH_MODULE(Class)    {RegistryGraphModuleFactory(#Class,new RegistryGraphModule<Class##Module>);}

VK_NAMESPACE_END