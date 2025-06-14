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
};//class RenderComponent

COMPONENT_NAMESPACE_END
