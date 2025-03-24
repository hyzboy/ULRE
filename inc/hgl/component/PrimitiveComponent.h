#pragma once

#include<hgl/component/RenderComponent.h>
#include<hgl/graph/VKRenderable.h>

COMPONENT_NAMESPACE_BEGIN

class PrimitiveComponent:public RenderComponent
{
public:

    PrimitiveComponent()=default;
    virtual ~PrimitiveComponent()=default;

    Renderable *GetRenderable();
};//class PrimitiveComponent

COMPONENT_NAMESPACE_END
