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
    RenderTarget *render_target;

public:

    const bool IsRender()const noexcept{return true;}

public:

    NO_COPY_NO_MOVE(RenderModule)

    RenderModule(const AnsiString &name):GraphModule(name){}
    virtual ~RenderModule()=default;
    
    virtual void OnRenderTarget(RenderTarget *rt)override{render_target=rt;}

    virtual void OnResize(const VkExtent2D &ext)override{current_extent=ext;}
};//class RenderModule
