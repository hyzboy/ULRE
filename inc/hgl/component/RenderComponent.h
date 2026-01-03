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
protected:

    RenderComponent(ComponentDataPtr cdp,ComponentManager *cm,size_t derived_type_hash)
        :GeometryComponent(cdp,cm,derived_type_hash)
    {
    }

public:

    virtual ~RenderComponent()=default;

    virtual const bool CanRender()const=0;                  ///<当前数据是否可以渲染

    // 由组件创建并提交其 DrawNode 到渲染集合。返回提交的节点数量。
    virtual uint SubmitDrawNodes(hgl::graph::RenderBatchMap &mrm)=0;

public:

    virtual U8String GetComponentInfo() const override;
};//class RenderComponent

COMPONENT_NAMESPACE_END
