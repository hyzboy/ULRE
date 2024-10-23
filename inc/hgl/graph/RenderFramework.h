#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/List.h>
#include<hgl/graph/ViewportInfo.h>

VK_NAMESPACE_BEGIN

class RenderModule;

/**
* 光照剔除模式
*/
enum class LightingCullingMode
{
    None,           ///<不剔除

    /*
    * 基于Tile的剔除模式
    * 按屏幕XY坐标划分成多个Tile，再配合znear/zfar形成一个Volume。所有光源和Volume计算相交性
    */
    Tile,           ///<瓦片剔除

    /**
    * 基于Tile的剔除模式的改进型
    * 同Tile方法，得出Tile后，再通过Compute Shader遍历Tile内所有象素，得出当前Tile的最远z值和最近z值。
    * 根据XY与zNear/zFar得出一个Volume，计算所有光源与Volume相交性。
    */
    TileVolume,     ///<瓦片体积剔除

    /**
    * 基于Tile的剔除模式的改进型
    * 同TileVolume方法得出Volume后，再将Volume按深度划分成多个Volume。
    * 剔除掉没有象素的Volume，再将剩下的Volume与光源计算相交性。
    */
    Cluster,        ///<集簇剔除

    ENUM_CLASS_RANGE(None,Cluster)
};//enum class LightingCullingMode

enum class BlendMode
{
    Opaque,
    Mask,
    Transparent,
    Additive,
    Subtractive
};

enum class RenderOrder
{
    First,          ///<最先渲染

    NearToFar,      ///<从近到远
    Irrorder,       ///<无序渲染
    FarToNear,      ///<从远到近

    Last,           ///<最后渲染

    ENUM_CLASS_RANGE(First,Last)
};//enum class RenderOrder

class GraphModule;

/**
 * 渲染模块工作配置
 */
class GraphModuleWorkConfig
{
    /**
     * 渲染模块名称
     * 在render_module为nullptr时，用于创建或加载RenderModule。
     * 它和RenderModule返回的名称有可能一样，但也有可能不一样。
     */
    AnsiString render_module_name;
    GraphModule *render_module=nullptr;

    BlendMode blend_mode;
    RenderOrder render_order;

public:

    const AnsiString &GetModuleName()const{return render_module_name;}                              ///<取得渲染模块名称

    GraphModule *GetModule(){return render_module;}                                                ///<取得渲染模块

public:

    GraphModuleWorkConfig();
    virtual ~GraphModuleWorkConfig()=default;
};//class GraphModuleWorkConfig

/**
* 渲染框架
*/
class RenderFramework
{
    ObjectList<GraphModule> module_list;

protected:

    ViewportInfo        viewport_info;

    int                 swap_chain_count    =0;

protected:

    GPUDevice *         device              =nullptr;
    RenderPass *        device_render_pass  =nullptr;
    RTSwapchain *       default_rt          =nullptr;

    RenderCmdBuffer **  render_cmd_buffers  =nullptr;

private:

    double              last_time           =0;
    double              cur_time            =0;

    uint64              frame_count         =0;

public:

    const uint64    GetFrameCount       ()const noexcept{return frame_count;}                       ///<取得当前帧数
    void            RestartFrameCount   ()noexcept{frame_count=0;}                                  ///<重新开始统计帧数

public:

    NO_COPY_NO_MOVE(RenderFramework)

    RenderFramework(){}
    virtual ~RenderFramework()=default;

    virtual void StartTime();

    /**
    * @pragma delta_time 本帧时间间隔
    */
    virtual void Update(const double delta_time){}

    virtual void BeginFrame(){};                                                                    ///<开始当前帧
    virtual void EndFrame(){};                                                                      ///<当前帧结束

    virtual void MainLoop();                                                                        ///<主循环
};//class RenderFramework

VK_NAMESPACE_END