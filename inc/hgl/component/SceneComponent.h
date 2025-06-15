#pragma once

#include<hgl/component/Component.h>
#include<hgl/graph/SceneOrient.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 场景组件<br>
* 场景组件中的元素必须是针对场景起作用的，并不一定需要自己绘出来，但也对场景产生影响。比如太阳光、全局风场
*/
class SceneComponent:public Component,public SceneOrient
{
public:

    using Component::Component;
    virtual ~SceneComponent()=default;

    virtual Component *Duplication() override
    {
        SceneComponent *sc=(SceneComponent *)Component::Duplication();

        if(!sc)
            return(sc);

        sc->SetLocalMatrix(GetLocalMatrix());
        return sc;
    }
};//class SceneComponent

COMPONENT_NAMESPACE_END
