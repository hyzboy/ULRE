#pragma once

#include<hgl/component/Component.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 可渲染组件
*/
class RenderComponent: public Component
{
public:

    using Component::Component;
    virtual ~RenderComponent()=default;
};//class RenderComponent

COMPONENT_NAMESPACE_END
