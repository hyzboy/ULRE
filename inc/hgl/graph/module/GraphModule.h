#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;

class GraphModule
{
    AnsiString module_name;

    bool module_enable;
    bool module_ready;

protected:

    virtual void SetModuleEnable(bool e){module_enable=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:

    const AnsiString &GetModuleName()const{return module_name;}

    const bool IsEnable ()const noexcept{return module_enable;}
    const bool IsReady  ()const noexcept{return module_ready;}

public:

    NO_COPY_NO_MOVE(GraphModule)

    GraphModule(const AnsiString &name){module_name=name;}
    virtual ~GraphModule()=default;

    virtual bool Init(){return true;}
    virtual void Execute(const double,RenderCmdBuffer *){}

};//class GraphModule

template<typename T> GraphModule *Create()
{
    return new T();
}

typedef GraphModule *(*GraphModuleCreateFunc)();

bool RegistryGraphModule(const AnsiString &,GraphModuleCreateFunc);
GraphModule *GetGraphModule(const AnsiString &);

VK_NAMESPACE_END