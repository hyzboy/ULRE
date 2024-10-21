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

    NO_COPY_NO_MOVE(RenderModule)

    RenderModule(const AnsiString &name):GraphModule(name){}
    virtual ~RenderModule()=default;

    virtual bool Init()override{return true;}

    virtual void Execute(const double,RenderCmdBuffer *)override{}

    virtual void OnResize(const VkExtent2D &ext){current_extent=ext;}
    virtual void OnFocusLost(){}

    virtual void OnPreFrame(){}
    virtual void OnPostFrame(){}
};//class RenderModule

