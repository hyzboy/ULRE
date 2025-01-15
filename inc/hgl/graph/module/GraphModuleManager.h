#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

class GraphModule;

class GraphModuleManager
{
    GPUDevice *device;

protected:

    List<GraphModule *> module_list;
    Map<size_t,GraphModule *> module_map;

public:

    GraphModuleManager(GPUDevice *dev){device=dev;}

    virtual ~GraphModuleManager();
    
public:

    GPUDevice *     GetDevice()                         {return device;}                                                ///<取得GPU设备

public:

    GraphModule *   GetModule(const size_t type_hash)   {return GetObjectFromList(module_map,type_hash);}               ///<取得指定类型的模块
    template<typename T>
    T *             GetModule()                         {return(GetModule(typeid(T).hash_code()));}                     ///<取得指定类型的模块

    bool ConatainsModule(const size_t &type_hash)const  {return module_map.ContainsKey(type_hash);}                     ///<确认是否包含指定类型的模块

    bool Registry(GraphModule *);                                                                                       ///<注册一个模块
    bool Unregistry(GraphModule *);                                                                                     ///<注销一个模块
};//class GraphModuleManager

VK_NAMESPACE_END
