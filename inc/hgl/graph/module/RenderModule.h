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
    
    const bool IsRender()const noexcept{return true;}

public:

    NO_COPY_NO_MOVE(RenderModule)

    using GraphModule::GraphModule;
    virtual ~RenderModule()=default;

    virtual void OnResize(const VkExtent2D &ext)override{current_extent=ext;}

    virtual void OnExecute(const double,RenderCmdBuffer *){}
};//class RenderModule

VK_NAMESPACE_END
