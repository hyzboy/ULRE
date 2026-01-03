#pragma once

#include<hgl/component/Component.h>
#include<hgl/graph/NodeTransform.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 场景组件<br>
* 场景组件中的元素必须是针对场景起作用的，并不一定需要自己绘出来，但也对场景产生影响。比如太阳光、全局风场
*/
class SceneComponent:public Component,public NodeTransform
{
protected:

    SceneComponent(ComponentDataPtr cdp,ComponentManager *cm,size_t derived_type_hash)
        :Component(cdp,cm,derived_type_hash)
    {
    }

public:

    virtual ~SceneComponent()=default;

    virtual Component *Clone() override
    {
        SceneComponent *sc=(SceneComponent *)Component::Clone();

        if(!sc)
            return(sc);

        sc->SetLocalMatrix(GetLocalMatrix());
        return sc;
    }

public:

    virtual U8String GetComponentInfo() const override;
};//class SceneComponent

COMPONENT_NAMESPACE_END
