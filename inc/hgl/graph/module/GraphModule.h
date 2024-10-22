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

    virtual const bool IsRender()=0;                                            ///<是否为渲染模块

    GraphModuleManager *GetManager(){return module_manager;}                    ///<取得模块管理器

    const AnsiString &GetModuleName()const{return module_name;}                 ///<取得模块名称

    const bool IsEnable ()const noexcept{return module_enable;}                 ///<当前模块是否启用
    const bool IsReady  ()const noexcept{return module_ready;}                  ///<当前模块是否准备好

public:

    NO_COPY_NO_MOVE(GraphModule)

    GraphModule(const AnsiString &name){module_name=name;}
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

public:

    GraphModuleManager(GPUDevice *dev){device=dev;}
    ~GraphModuleManager();

    bool Registry(const AnsiString &name,GraphModule *gm);
    GraphModule *GetModule(const AnsiString &name);

public: //事件

    void OnResize(const VkExtent2D &);
};//class GraphModuleManager

GraphModuleManager *GetGraphModuleManager(GPUDevice *);

VK_NAMESPACE_END
