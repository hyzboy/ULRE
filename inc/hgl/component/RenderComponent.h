#pragma once

#include<hgl/component/GeometryComponent.h>

// forward declarations to avoid heavy includes
namespace hgl { namespace graph { class RenderBatchMap; } }

COMPONENT_NAMESPACE_BEGIN

/**
* 可渲染组件
*/
class RenderComponent:public GeometryComponent
{
public:

    using GeometryComponent::GeometryComponent;
    virtual ~RenderComponent()=default;

    virtual const bool CanRender()const=0;                  ///<当前数据是否可以渲染

    // 由组件创建并提交其 DrawNode 到渲染集合。返回提交的节点数量。
    virtual uint SubmitDrawNodes(hgl::graph::RenderBatchMap &mrm)=0;

public:

    virtual U8String GetComponentInfo() const override;
};//class RenderComponent

COMPONENT_NAMESPACE_END
