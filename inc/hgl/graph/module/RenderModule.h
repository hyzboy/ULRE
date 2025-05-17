#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/Size2.h>

VK_NAMESPACE_BEGIN

/**
* 渲染模块基类
*/
class RenderModule:public GraphModule
{
public:

    NO_COPY_NO_MOVE(RenderModule)

    using GraphModule::GraphModule;
    virtual ~RenderModule()=default;

    virtual bool OnFrameRender(const double,RenderCmdBuffer *)=0;                                   ///<帧绘制回调
};//class RenderModule

#define RENDER_MODULE_CLASS(class_name) class class_name:public GraphModuleInherit<class_name,RenderModule>

#define RENDER_MODULE_CONSTRUCT(class_name) class_name::class_name(VulkanDevice *dev):GraphModuleInherit<class_name,RenderModule>(dev,#class_name)

VK_NAMESPACE_END
