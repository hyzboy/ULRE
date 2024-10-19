#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/Size2.h>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;

/**
* 渲染模块基类
*/
class RenderModule
{
    OSString module_name;

    VkExtent2D current_extent;

    bool module_enable;
    bool module_ready;

protected:

    virtual void SetModuleEnable(bool e){module_enable=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:

    const OSString &GetModuleName()const{return module_name;}

    const bool IsEnable ()const noexcept{return module_enable;}
    const bool IsReady  ()const noexcept{return module_ready;}

public:

    NO_COPY_NO_MOVE(RenderModule)

    RenderModule(const OSString &name)
    {
        module_name=name;
    }

    virtual ~RenderModule()=default;

    virtual bool Init(){return true;}

    virtual void Execute(const double,RenderCmdBuffer *){}

    virtual void OnResize(const VkExtent2D &ext){current_extent=ext;}
    virtual void OnFocusLost(){}

    virtual void OnPreFrame(){}
    virtual void OnPostFrame(){}
};//class RenderModule

template<typename T> RenderModule *Create()
{
    return new T();
}

typedef RenderModule *(*RenderModuleCreateFunc)();

bool RegistryRenderModule(const OSString &,RenderModuleCreateFunc);
RenderModule *GetRenderModule(const OSString &);

VK_NAMESPACE_END
