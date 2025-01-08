#pragma once

#include<hgl/graph/RenderFramework.h>
#include<initializer_list>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;

class RenderFramework;

using GraphModuleHashList=std::initializer_list<size_t>;

class GraphModule
{
    RenderFramework *render_framework;

    AnsiIDName module_name;

    bool module_inited;
    bool module_enabled;
    bool module_ready;

protected:

    template<typename T>
    T *             GetModule(bool create=false){return render_framework->GetModule<T>(create);}                        ///<获取指定类型的模块
    GraphModule *   GetModule(const AnsiIDName &name,bool create=false);                                                ///<获取指定名称的模块

protected:

    virtual void SetModuleEnabled(bool e){module_enabled=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:
    
            GPUDevice *         GetDevice           ()      {return render_framework->GetDevice();}                     ///<取得GPU设备
            VkDevice            GetVkDevice         ()const {return render_framework->GetVkDevice();}                   ///<取得VkDevice
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const {return render_framework->GetPhysicalDevice();}             ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  ()      {return render_framework->GetDeviceAttribute();}            ///<取得设备属性

            RenderFramework *   GetFramework        ()      {return render_framework;}                                  ///<取得渲染框架

            const AnsiIDName &  GetName             ()const {return module_name;}                                       ///<取得模块名称(标准通用的名称，比如Upscale，供通用模块使用)
    virtual const AnsiIDName &  GetFullName         ()const {return module_name;}                                       ///<取得名称(完整的私有名称，比如FSR3Upscale,DLSS3Upscale)

    virtual const bool          IsPerFrame          ()      {return false;}                                             ///<是否每帧运行
    virtual const bool          IsRender            ()      {return false;}                                             ///<是否为渲染模块

            const bool          IsInited            ()const         {return module_inited;}                             ///<是否已经初始化
            const bool          IsEnabled           ()const noexcept{return module_enabled;}                            ///<当前模块是否启用
            const bool          IsReady             ()const noexcept{return module_ready;}                              ///<当前模块是否准备好

public:

    NO_COPY_NO_MOVE(GraphModule)

protected:

    GraphModule(RenderFramework *rf,const AnsiIDName &name);

public:

    virtual ~GraphModule();

    virtual const size_t GetTypeHash()const=0;

    static const GraphModuleHashList GetDependentModules(){return {};}                                                          ///<取得依赖的模块列表

public: //回调事件

    virtual void OnRenderTarget(RenderTarget *){}                                                   ///<设置渲染目标
    virtual void OnResize(const VkExtent2D &){}                                                     ///<窗口大小改变

    virtual void OnPreFrame(){}                                                                     ///<帧绘制前回调
    virtual void OnPostFrame(){}                                                                    ///<帧绘制后回调
};//class GraphModule

using GraphModuleMapByIDName=Map<AnsiIDName,GraphModule *>;

//template<typename T,typename BASE> class GraphModuleInherit:public BASE
//{
//public:
//
//    GraphModuleInherit(RenderFramework *rf):BASE(rf,typeid(T))
//    {}
//
//    static const size_t StaticHash()
//    {
//        return typeid(T).hash_code();
//    }
//
//};//class GraphModuleInherit

#define GRAPH_MODULE_CREATE_FUNC(name) name *name::CreateModule(RenderFramework *rf,GraphModuleMapByIDName &dep_modules)

#define GRAPH_BASE_MODULE_CONSTRUCT(name,base_class)    public:\
    NO_COPY_NO_MOVE(name)   \
    static const size_t StaticHash(){return typeid(name).hash_code();}    \
           const size_t GetTypeHash()const override{return name::StaticHash();}    \
    static const AnsiIDName &GetModuleName()    \
    {   \
        static const AnsiIDName id_name(#name);    \
        return id_name;    \
    }   \
    \
    private:    \
        name(RenderFramework *rf):base_class(rf,GetModuleName()){} \
    \
    public: \
        static name *CreateModule(RenderFramework *rf,GraphModuleMapByIDName &);    \
        static const GraphModuleHashList GetDependentModules();


#define GRAPH_MODULE_CONSTRUCT(name)    GRAPH_BASE_MODULE_CONSTRUCT(name,GraphModule)
#define RENDER_MODULE_CONSTRUCT(name)   GRAPH_BASE_MODULE_CONSTRUCT(name,RenderModule)

VK_NAMESPACE_END
