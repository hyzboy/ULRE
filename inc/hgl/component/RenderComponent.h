#pragma once

#include<hgl/component/Component.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 可渲染组件
*/
class RenderComponent: public Component
{
public:

    RenderComponent(SceneNode *psn,ComponentData *cd,ComponentManager *cm)
        :Component(psn,cd,cm){}
    virtual ~RenderComponent()=default;
};//class RenderComponent

COMPONENT_NAMESPACE_END
