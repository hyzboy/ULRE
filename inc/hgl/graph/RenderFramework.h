#pragma once

#include<hgl/graph/BlendMode.h>
#include<hgl/type/List.h>
#include<hgl/graph/ViewportInfo.h>
#include<hgl/graph/module/GraphModule.h>
#include<hgl/io/event/WindowEvent.h>

VK_NAMESPACE_BEGIN

class GPUDevice;
class TileData;
class TileFont;
class FontSource;

class Window;
class VulkanInstance;

class RenderPassManager;
class TextureManager;

class SwapchainModule;

/**
* 渲染框架
*/
class RenderFramework:public io::WindowEvent
{
protected:

    Window *            win                 =nullptr;
    VulkanInstance *    inst                =nullptr;

    GPUDevice *         device              =nullptr;

protected:

    GraphModuleManager *graph_module_manager=nullptr;

    ObjectList<GraphModule> module_list;

protected:

    SwapchainModule *   swapchain_module    =nullptr;

protected:

    RenderPassManager * render_pass_manager =nullptr;
    TextureManager *    texture_manager     =nullptr;

protected:

    ViewportInfo        viewport_info;

private:

    double              last_time           =0;
    double              cur_time            =0;

    uint64              frame_count         =0;

public:

    const uint64    GetFrameCount       ()const noexcept{return frame_count;}                       ///<取得当前帧数
    void            RestartFrameCount   ()noexcept{frame_count=0;}                                  ///<重新开始统计帧数

public: //module

    template<typename T> T *GetModule(){return graph_module_manager->GetModule<T>(false);}          ///<获取指定类型的模块

    SwapchainModule *GetSwapchain(){return swapchain_module;}                                       ///<取得Swapchain模块

public: //manager

    RenderPassManager * GetRenderPassManager(){return render_pass_manager;}                         ///<取得渲染通道管理器
    TextureManager *    GetTextureManager   (){return texture_manager;}                             ///<取得纹理管理器

public: //event

    virtual void OnResize(uint,uint)override;
    virtual void OnActive(bool)override;
    virtual void OnClose ()override;

public:

    NO_COPY_NO_MOVE(RenderFramework)

    RenderFramework();
    virtual ~RenderFramework();

    virtual bool Init();                                                                            ///<初始化

    virtual void StartTime();

    /**
    * @pragma delta_time 本帧时间间隔
    */
    virtual void Update(const double delta_time){}

    virtual void BeginFrame();                                                                      ///<开始当前帧
    virtual void EndFrame();                                                                        ///<当前帧结束

    virtual void MainLoop();                                                                        ///<主循环

public: //TileData

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体
};//class RenderFramework

VK_NAMESPACE_END
