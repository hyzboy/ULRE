#pragma once

#include<hgl/vk/VK.h>
#include<hgl/type/UnorderedMap.h>

VK_NAMESPACE_BEGIN

class GraphModule;
class IGraphicsContext;

class GraphModuleManager
{
    RenderFramework *render_framework;
    IGraphicsContext *graphics_context=nullptr;

protected:

    ValueArray<GraphModule *> module_list;
    UnorderedMap<size_t,GraphModule *> module_map;

public:

    GraphModuleManager(RenderFramework *rf){render_framework=rf;}

    virtual ~GraphModuleManager();

public:

    RenderFramework *   GetRenderFramework  ()const{return render_framework;}                                           ///<取得渲染框架
    IGraphicsContext *  GetGraphicsContext  ()const{return graphics_context;}                                          ///<取得GraphicsContext
    void                SetGraphicsContext  (IGraphicsContext *gc);                                                    ///<设置GraphicsContext
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

        T *result=new T(render_framework);

        if(graphics_context)
            result->SetGraphicsContext(graphics_context);

        Register(result);

        return result;
    }
};//class GraphModuleManager

VK_NAMESPACE_END
