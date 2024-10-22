#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class RenderCmdBuffer;
class GraphModuleManager;

class GraphModule
{
    GraphModuleManager *module_manager;

    AnsiString module_name;

    bool module_enable;
    bool module_ready;

protected:

    virtual void SetModuleEnable(bool e){module_enable=e;}
    virtual void SetModuleReady(bool r){module_ready=r;}

public:

    virtual const bool IsRender(){return false;}                                ///<是否为渲染模块

    GraphModuleManager *GetManager(){return module_manager;}                    ///<取得模块管理器

    static const char *GetModuleName(){return nullptr;}                         ///<取得模块名称(标准通用的名称，比如Upscale，供通用模块使用)
    virtual const char *GetName()const{return module_name.c_str();}             ///<取得名称(完整的私有名称，比如FSR3Upscale,DLSS3Upscale)

    const bool IsEnable ()const noexcept{return module_enable;}                 ///<当前模块是否启用
    const bool IsReady  ()const noexcept{return module_ready;}                  ///<当前模块是否准备好

public:

    NO_COPY_NO_MOVE(GraphModule)

    GraphModule(GraphModuleManager *gmm,const AnsiString &name)
    {
        module_manager=gmm;
        module_name=name;
    }
    virtual ~GraphModule()=default;

    virtual bool Init(){return true;}                                           ///<初始化当前模块

public: //回调事件

    virtual void OnRenderTarget(RenderTarget *){}                               ///<设置渲染目标
    virtual void OnResize(const VkExtent2D &){}                                 ///<窗口大小改变

    virtual void OnPreFrame(){}                                                 ///<帧绘制前回调
    virtual void OnExecute(const double,RenderCmdBuffer *){}
    virtual void OnPostFrame(){}                                                ///<帧绘制后回调
};//class GraphModule

class GraphModuleManager
{
    GPUDevice *device;

    Map<AnsiString,GraphModule *> graph_module_map;

protected:
    
    GraphModule *GetModule(const AnsiString &name,bool create);

public:

    GraphModuleManager(GPUDevice *dev){device=dev;}
    ~GraphModuleManager();

    /**
    * 获取指定类型的模块
    * @param create 如果不存在，是否创建新的
    */
    template<typename T> T *GetModule(bool create=false)
    {
        return (T *)GetModule(T::GetModuleName(),create);
    }

public: //事件

    void OnResize(const VkExtent2D &);
};//class GraphModuleManager

GraphModuleManager *GetGraphModuleManager(GPUDevice *);

VK_NAMESPACE_END
