#pragma once

#include<hgl/component/RenderComponent.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 图元组件<br>
* 组件中的元素必须是一个可以明确描述的几何体，可以被明确标记尺寸、参与空间、物理计算等。
*/
class PrimitiveComponent:public RenderComponent
{
public:

    PrimitiveComponent(SceneNode *psn,ComponentData *cd,ComponentManager *cm)
        :RenderComponent(psn,cd,cm){}
    virtual ~PrimitiveComponent()=default;
};//class PrimitiveComponent

COMPONENT_NAMESPACE_END
