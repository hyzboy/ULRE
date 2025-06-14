#pragma once

#include<hgl/component/Component.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 场景组件<br>
* 场景组件中的元素必须是针对场景起作用的，并不一定需要自己绘出来，但也对场景产生影响。比如太阳光、全局风场
*/
class SceneComponent:public Component
{
public:

    using Component::Component;

    virtual ~SceneComponent()=default;
};//class SceneComponent

COMPONENT_NAMESPACE_END
