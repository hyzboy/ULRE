#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

class GraphModuleManager
{
    RenderFramework *render_framework;

protected:

    ArrayList<GraphModule *> module_list;
    Map<size_t,GraphModule *> module_map;

public:

    GraphModuleManager(RenderFramework *rf){render_framework=rf;}

    virtual ~GraphModuleManager();
    
public:

    RenderFramework *   GetRenderFramework  ()const{return render_framework;}                                           ///<取得渲染框架
    VulkanDevice *      GetDevice           ()const;                                                                    ///<取得GPU设备

public:

    GraphModule *   Get(const size_t type_hash)   {return GetObjectFromMap(module_map,type_hash);}                     ///<取得指定类型的模块
    template<typename T>
    T *             Get()                         {return((T *)Get(typeid(T).hash_code()));}                            ///<取得指定类型的模块

    bool Contains(const size_t &type_hash)const   {return module_map.ContainsKey(type_hash);}                           ///<确认是否包含指定类型的模块

    template<typename T>
    bool Contains()const{return Contains(typeid(T).hash_code());}                                                       ///<确认是否包含指定类型的模块

    bool Register(GraphModule *);                                                                                       ///<注册一个模块
    bool Unregister(GraphModule *);                                                                                     ///<注销一个模块

    template<typename T>
    T *GetOrCreate()                                                                                                    ///<注册一个模块
    {
        if(Contains<T>())
            return Get<T>();

        T *result=new T(render_framework);

        Register(result);

        return result;
    }
};//class GraphModuleManager

VK_NAMESPACE_END
