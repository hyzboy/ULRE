#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/TypeInfo.h>

VK_NAMESPACE_BEGIN

class TextureManager;
class RenderTargetManager;
class RenderPassManager;

class GraphModule
{
    RenderFramework *render_framework;

public:
    
            RenderFramework *   GetRenderFramework  ()const{return render_framework;}               ///<取得渲染框架
            GPUDevice *         GetDevice           ();                                             ///<取得GPU设备
            VkDevice            GetVkDevice         ()const;                                        ///<取得VkDevice
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const;                                        ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  ()const;                                        ///<取得设备属性
            VkPipelineCache     GetPipelineCache    ()const;                                        ///<取得PipelineCache

public:

    virtual void OnResize(const VkExtent2D &){};                                                    ///<窗口大小改变

public:

    GraphModule(RenderFramework *rf){render_framework=rf;}
    virtual ~GraphModule()=default;

    virtual const size_t GetTypeHash()const noexcept=0;
    virtual const AnsiString &GetName()const=0;
};//class GraphModule

template<typename T,typename BASE> class GraphModuleInherit:public BASE
{
    AnsiString manager_name;

public:

    const size_t GetTypeHash()const noexcept override
    {
        return typeid(T).hash_code();
    }

    const AnsiString &GetName()const override
    {
        return manager_name;
    }

public:

    GraphModuleInherit(RenderFramework *rf,const AnsiString &name):BASE(rf)
    {
        manager_name=name;
    }

    virtual ~GraphModuleInherit()=default;
};//class GraphModuleInherit

#define GRAPH_MODULE_CLASS(class_name) class class_name:public GraphModuleInherit<class_name,GraphModule>

#define GRAPH_MODULE_CONSTRUCT(class_name) class_name::class_name(RenderFramework *rf):GraphModuleInherit<class_name,GraphModule>(rf,#class_name)

VK_NAMESPACE_END
