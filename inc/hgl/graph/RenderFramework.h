#pragma once

#include<hgl/graph/BlendMode.h>
#include<hgl/type/List.h>
#include<hgl/graph/ViewportInfo.h>
#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class GPUDevice;
class TileData;
class TileFont;
class FontSource;

//
///**
// * 渲染模块工作配置
// */
//class GraphModuleWorkConfig
//{
//    /**
//     * 渲染模块名称
//     * 在render_module为nullptr时，用于创建或加载RenderModule。
//     * 它和RenderModule返回的名称有可能一样，但也有可能不一样。
//     */
//    AnsiString render_module_name;
//    GraphModule *render_module=nullptr;
//
//    BlendMode blend_mode;
//    RenderOrder render_order;
//
//public:
//
//    const AnsiString &GetModuleName()const{return render_module_name;}                              ///<取得渲染模块名称
//
//    GraphModule *GetModule(){return render_module;}                                                ///<取得渲染模块
//
//public:
//
//    GraphModuleWorkConfig();
//    virtual ~GraphModuleWorkConfig()=default;
//};//class GraphModuleWorkConfig

class Window;
class VulkanInstance;

class TextureManager;

class SwapchainModule;

struct RenderFrameworkInitConfig
{

};//struct RenderFrameworkInitConfig

/**
* 渲染框架
*/
class RenderFramework
{
protected:

    Window *            win                 =nullptr;
    VulkanInstance *    inst                =nullptr;

    GPUDevice *         device              =nullptr;

protected:

    GraphModuleManager *graph_module_manager=nullptr;

    ObjectList<GraphModule> module_list;

    SwapchainModule *   swapchain_module    =nullptr;

protected:

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

    TextureManager *GetTextureManager(){return texture_manager;}                                    ///<取得纹理管理器

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

    virtual void BeginFrame(){};                                                                    ///<开始当前帧
    virtual void EndFrame(){};                                                                      ///<当前帧结束

    virtual void MainLoop();                                                                        ///<主循环

public: //TileData

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集
    
    TileFont *CreateTileFont(FontSource *fs,int limit_count=-1);                                                        ///<创建一个Tile字体
};//class RenderFramework

VK_NAMESPACE_END
