#pragma once

#include<hgl/component/Component.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 可渲染组件
*/
class RenderComponent: public Component
{
public:

    RenderComponent()=default;
    virtual ~RenderComponent()=default;
};//class RenderComponent

COMPONENT_NAMESPACE_END
