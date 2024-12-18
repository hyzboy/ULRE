#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/type/IDName.h>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;
class GraphModule;

class RenderFramework;

struct GraphModulesMap
{
    SortedSet<GraphModule *> gm_set;

    Map<AnsiIDName,GraphModule *> gm_map_by_name;
    Map<size_t,GraphModule *> gm_map_by_hash;

public:

    bool Add(GraphModule *gm);

    const bool IsEmpty()const
    {
        return gm_set.IsEmpty();
    }

    GraphModule *Get(const AnsiIDName &name)const
    {
        GraphModule *gm;

        if(gm_map_by_name.Get(name,gm))
            return gm;

        return nullptr;
    }

    template<typename T>
    const bool Get(T *&gm)const
    {
        return gm_map_by_hash.Get(GetTypeHash<T>(),gm);
    }

    const bool IsLoaded(const AnsiIDName &name)const{return gm_map_by_name.ContainsKey(name);}

    template<typename T>
    const bool IsLoaded()const{return gm_map_by_hash.ContainsKey(T::GetTypeHash());}
};

class GraphModuleManager
{
    GPUDevice *device;
    RenderFramework *framework;

    GraphModulesMap graph_module_map;                   ///<模块映射表

public:

    GraphModuleManager(RenderFramework *);
    virtual ~GraphModuleManager();

            GPUDevice *         GetDevice           ()noexcept  {return device;}                                        ///<取得GPU设备
            VkDevice            GetVkDevice         ()const     {return device->GetDevice();}
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const     {return device->GetPhysicalDevice();}                   ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  ()          {return device->GetDeviceAttribute();}                  ///<取得设备属性

            RenderFramework *   GetFramework        ()          {return framework;}                                     ///<取得渲染框架

    /**
     * 获取指定名称的模块
     * @param create 如果不存在，是否创建新的
     */
    GraphModule *GetModule(const AnsiIDName &name,bool create);                                     ///<获取指定名称的模块

    /**
     * 获取指定类型的模块
     * @param create 如果不存在，是否创建新的
     */
    template<typename T>
    T *GetModule(bool create=false){return (T *)GetModule(T::GetModuleName(),create);}              ///<获取指定类型的模块

    void ReleaseModule(GraphModule *);                                                              ///<释放指定模块

public: //事件

    void OnResize(const VkExtent2D &);
};//class GraphModuleManager

GraphModuleManager *GetGraphModuleManager(RenderFramework *);

class GraphModule:public Comparator<GraphModule>
{
    GraphModuleManager *module_manager;

    AnsiIDName module_name;
    AnsiIDName module_fullname;

    bool module_inited;
    bool module_enabled;
    bool module_ready;

    GraphModulesMap dependent_modules;

protected:

    //GraphModule *GetModule(const AnsiIDName &name){return module_manager->GetModule(name,true);}                      ///<获取指定名称的模块

    //template<typename T>
    //T *GetModule(){return (T *)GetModule(T::GetModuleName());}                                                        ///<获取指定类型的模块

protected:

    virtual void SetModuleEnabled(bool e){module_enabled=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:

            GraphModuleManager *GetManager          ()      {return module_manager;}                                    ///<取得模块管理器
            GPUDevice *         GetDevice           ()      {return module_manager->GetDevice();}                       ///<取得GPU设备
            VkDevice            GetVkDevice         ()const {return module_manager->GetVkDevice();}                     ///<取得VkDevice
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const {return module_manager->GetPhysicalDevice();}               ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  ()      {return module_manager->GetDeviceAttribute();}              ///<取得设备属性

            RenderFramework *   GetFramework        ()      {return module_manager->GetFramework();}                    ///<取得渲染框架

            const AnsiIDName &  GetName             ()const {return module_name;}                                       ///<取得模块名称(标准通用的名称，比如Upscale，供通用模块使用)
            const AnsiIDName &  GetFullName         ()const {return module_fullname;}                                   ///<取得名称(完整的私有名称，比如FSR3Upscale,DLSS3Upscale)

    virtual const bool          IsPerFrame          ()      {return false;}                                             ///<是否每帧运行
    virtual const bool          IsRender            ()      {return false;}                                             ///<是否为渲染模块

            const bool          IsInited            ()const         {return module_inited;}                             ///<是否已经初始化
            const bool          IsEnabled           ()const noexcept{return module_enabled;}                            ///<当前模块是否启用
            const bool          IsReady             ()const noexcept{return module_ready;}                              ///<当前模块是否准备好

public:

    NO_COPY_NO_MOVE(GraphModule)

    GraphModule(GraphModuleManager *gmm,const AnsiIDName &name,const AnsiIDName &fullname);
    virtual ~GraphModule();

    virtual const size_t GetTypeHash()const=0;

    virtual bool Init(GraphModulesMap *);                                                                               ///<初始化当前模块

    static const AnsiIDNameSet &GetDependentModules()                                                                   ///<取得依赖的模块列表
    {
        static const AnsiIDNameSet empty;

        return empty;
    }

    const int compare(const GraphModule &gm)const override
    {
        auto &dependent_modules_name=GetDependentModules();

        return(dependent_modules_name.Contains(gm.module_name)?1:-1);    //如果我依赖于他，那么我比他大
    }

public:

    GraphModule *   GetDependentModule(const AnsiIDName &name){return dependent_modules.Get(name);}                     ///<获取指定名称的模块

    template<typename T>
    T *             GetDependentModule(){return dependent_modules.Get<T>();}                                            ///<获取指定类型的模块

public: //回调事件

    virtual void OnRenderTarget(RenderTarget *){}                                                   ///<设置渲染目标
    virtual void OnResize(const VkExtent2D &){}                                                     ///<窗口大小改变

    virtual void OnPreFrame(){}                                                                     ///<帧绘制前回调
    virtual void OnPostFrame(){}                                                                    ///<帧绘制后回调
};//class GraphModule

#define GRAPH_MODULE_CONSTRUCT(name)    public:\
    NO_COPY_NO_MOVE(name)   \
    static const size_t StaticHash(){return typeid(name).hash_code();}    \
           const size_t GetTypeHash()const override{return name::StaticHash();}    \
    static const AnsiIDName &GetModuleName()    \
    {   \
        static const AnsiIDName id_name(#name);    \
        return id_name;    \
    }   \
    \
    name(GraphModuleManager *gmm):GraphModule(gmm,GetModuleName()){}

#define RENDER_MODULE_CONSTRUCT(name)    public:\
    static const size_t StaticHash(){return typeid(name).hash_code();}    \
           const size_t GetTypeHash()const override{return name::StaticHash();}    \
    NO_COPY_NO_MOVE(name)   \
    static const AnsiIDName &GetModuleName()    \
    {   \
        static const AnsiIDName id_name(#name);    \
        return id_name;    \
    }   \
    \
    name(GraphModuleManager *gmm):RenderModule(gmm,GetModuleName()){}

VK_NAMESPACE_END
