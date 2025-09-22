#pragma once

#include<hgl/component/PrimitiveComponent.h>

// forward declarations to avoid heavy includes
namespace hgl { namespace graph { class RenderBatchMap; } }

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
    virtual uint SubmitDrawNodes(hgl::graph::RenderBatchMap &mrm)=0;
};//class RenderComponent

COMPONENT_NAMESPACE_END
