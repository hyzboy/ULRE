#pragma once

#include<hgl/component/PrimitiveComponent.h>

// forward declarations to avoid heavy includes
namespace hgl { namespace graph { class MaterialRenderMap; class VulkanDevice; } }

COMPONENT_NAMESPACE_BEGIN

/**
* 可渲染组件
*/
class RenderComponent:public PrimitiveComponent
{
public:

    using PrimitiveComponent::PrimitiveComponent;
    virtual ~RenderComponent()=default;

    virtual const bool CanRender()const=0;                  ///<当前数据是否可以渲染

    // 由组件创建并提交其 DrawNode 到渲染集合。返回提交的节点数量。
    // 要求：Begin/End 由 RenderCollector 驱动，接口内只负责 Get/Add 对应的批并提交节点。
    virtual uint SubmitDrawNodes(hgl::graph::MaterialRenderMap &mrm, hgl::graph::VulkanDevice *device)=0;
};//class RenderComponent

COMPONENT_NAMESPACE_END
