#pragma once

#include<hgl/component/PrimitiveComponent.h>

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
};//class RenderComponent

COMPONENT_NAMESPACE_END
