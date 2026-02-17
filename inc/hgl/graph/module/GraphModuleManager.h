#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph{

class GraphModule;
class GraphicsContext;

class GraphModuleManager
{
    GraphicsContext *graphics_context=nullptr;

protected:

    ValueArray<GraphModule *> module_list;
    UnorderedMap<size_t,GraphModule *> module_map;

public:

    GraphModuleManager()=default;

    virtual ~GraphModuleManager();

public:

    GraphicsContext *   GetGraphicsContext  ()const{return graphics_context;}                                          ///<取得GraphicsContext
    void                SetGraphicsContext  (GraphicsContext *gc);                                                    ///<设置GraphicsContext
    VulkanDevice *      GetDevice           ()const;                                                                    ///<取得GPU设备

public:

    GraphModule *   Get(const size_t type_hash)
    {
        GraphModule** ptr = module_map.GetValuePointer(type_hash);
        return ptr ? *ptr : nullptr;
    }                     ///<取得指定类型的模块
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

        T *result=new T(graphics_context);

        if(graphics_context)
            result->SetGraphicsContext(graphics_context);

        Register(result);

        return result;
    }
};//class GraphModuleManager

}//namespace hgl::graph
