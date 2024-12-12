#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/type/IDName.h>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;
class GraphModule;

class GraphModuleManager
{
    GPUDevice *device;

    List<GraphModule *> module_list;                    ///<模块列表

    Map<AnsiIDName,GraphModule *> graph_module_map;     ///<模块映射表

public:

    GraphModuleManager(GPUDevice *dev){device=dev;}
    ~GraphModuleManager();

            GPUDevice *         GetDevice           ()noexcept{return device;}                      ///<取得GPU设备
            VkDevice            GetVkDevice         ()const{return device->GetDevice();}
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const{return device->GetPhysicalDevice();}    ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  (){return device->GetDeviceAttribute();}        ///<取得设备属性

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

    const bool IsLoaded(const AnsiIDName &name){return graph_module_map.ContainsKey(name);}         ///<是否已经加载了指定类型的模块

    template<typename T>
    const bool IsLoaded(){return graph_module_map.ContainsKey(T::GetModuleName());}                 ///<是否已经加载了指定类型的模块

public: //事件

    void OnResize(const VkExtent2D &);
};//class GraphModuleManager

GraphModuleManager *GetGraphModuleManager(GPUDevice *);

class GraphModule:public Comparator<GraphModule>
{
    GraphModuleManager *module_manager;

    AnsiIDName module_name;

    SortedSet<AnsiIDName> dependent_module;                                                         ///<依赖的模块

    bool module_init;
    bool module_enable;
    bool module_ready;

protected:

    virtual void SetModuleEnable(bool e){module_enable=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:

            GraphModuleManager *GetManager          (){return module_manager;}                                          ///<取得模块管理器
            GPUDevice *         GetDevice           (){return module_manager->GetDevice();}                             ///<取得GPU设备
            VkDevice            GetVkDevice         ()const{return module_manager->GetVkDevice();}                      ///<取得VkDevice
    const   GPUPhysicalDevice * GetPhysicalDevice   ()const{return module_manager->GetPhysicalDevice();}                ///<取得物理设备
            GPUDeviceAttribute *GetDeviceAttribute  (){return module_manager->GetDeviceAttribute();}                    ///<取得设备属性

    static  const AnsiIDName *GetModuleName(){return nullptr;}                                                          ///<取得模块名称(标准通用的名称，比如Upscale，供通用模块使用)
    virtual const AnsiIDName *GetName()const{return &module_name;}                                                      ///<取得名称(完整的私有名称，比如FSR3Upscale,DLSS3Upscale)
    
    virtual const bool IsRender (){return false;}                                                                       ///<是否为渲染模块

    const bool IsInit   ()const{return module_init;}                                                                    ///<是否已经初始化
    const bool IsEnable ()const noexcept{return module_enable;}                                                         ///<当前模块是否启用
    const bool IsReady  ()const noexcept{return module_ready;}                                                          ///<当前模块是否准备好

public:

    NO_COPY_NO_MOVE(GraphModule)

    GraphModule(GraphModuleManager *gmm,const AnsiIDName &name);
    virtual ~GraphModule();

    virtual bool Init(){module_init=true;return true;}                                                                  ///<初始化当前模块

    const int compare(const GraphModule &gm)const override
    {
        return(dependent_module.Contains(gm.module_name)?1:-1);    //如果我依赖于他，那么我比他大
    }

public:

    GraphModule *   GetModule(const AnsiIDName &name,bool create=false){return module_manager->GetModule(name,create);} ///<获取指定名称的模块

    template<typename T>
    T *             GetModule(bool create=false){return module_manager->GetModule<T>(create);}                          ///<获取指定类型的模块

public: //回调事件

    virtual void OnRenderTarget(RenderTarget *){}                                                   ///<设置渲染目标
    virtual void OnResize(const VkExtent2D &){}                                                     ///<窗口大小改变

    virtual void OnPreFrame(){}                                                                     ///<帧绘制前回调
    virtual void OnExecute(const double,RenderCmdBuffer *){}
    virtual void OnPostFrame(){}                                                                    ///<帧绘制后回调
};//class GraphModule

#define GRAPH_MODULE_CONSTRUCT(name)    public:\
    NO_COPY_NO_MOVE(name)   \
    static const AnsiIDName &GetModuleName()    \
    {   \
        static const AnsiIDName id_name(#name);    \
        return id_name;    \
    }   \
    \
    name(GraphModuleManager *gmm):GraphModule(gmm,GetModuleName()){}

#define RENDER_MODULE_CONSTRUCT(name)    public:\
    NO_COPY_NO_MOVE(name)   \
    static const AnsiIDName &GetModuleName()    \
    {   \
        static const AnsiIDName id_name(#name);    \
        return id_name;    \
    }   \
    \
    name(GraphModuleManager *gmm):RenderModule(gmm,GetModuleName()){}

VK_NAMESPACE_END
