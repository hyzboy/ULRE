#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Size2.h>

VK_NAMESPACE_BEGIN

/**
* 渲染模块基类
*/
class RenderModule:public GraphModule
{
    VkExtent2D current_extent;

public:
    
    const bool IsPerFrame   ()const noexcept{return true;}                                          ///<每帧运行
    const bool IsRender     ()const noexcept{return true;}                                          ///<渲染模块

public:

    NO_COPY_NO_MOVE(RenderModule)

    using GraphModule::GraphModule;
    virtual ~RenderModule()=default;

    virtual void OnResize(const VkExtent2D &ext)override{current_extent=ext;}

    virtual void OnFrameRender(const double,RenderCmdBuffer *)=0;                                   ///<帧绘制回调
};//class RenderModule

VK_NAMESPACE_END
