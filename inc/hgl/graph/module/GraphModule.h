#pragma once

#include<hgl/graph/VKDevice.h>
#include<hgl/type/TypeInfo.h>

VK_NAMESPACE_BEGIN

class GraphModule
{
    GPUDevice *device;

public:
    
            GPUDevice *         GetDevice           ()      {return device;}                        ///<取得GPU设备
            VkDevice            GetVkDevice         ()const {return device->GetDevice();}           ///<取得VkDevice
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const {return device->GetPhysicalDevice();}   ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  ()      {return device->GetDeviceAttribute();}  ///<取得设备属性

public:

    GraphModule(GPUDevice *dev){device=dev;}
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

    GraphModuleInherit(GPUDevice *dev,const AnsiString &name):BASE(dev)
    {
        manager_name=name;
    }

    virtual ~GraphModuleInherit()=default;
};//class GraphModuleInherit

#define GRAPH_MODULE_CLASS(class_name) class class_name:public GraphModuleInherit<class_name,GraphModule>

#define GRAPH_MODULE_CONSTRUCT(class_name) class_name::class_name(GPUDevice *dev):GraphModuleInherit<class_name,GraphModule>(dev,#class_name)

VK_NAMESPACE_END
