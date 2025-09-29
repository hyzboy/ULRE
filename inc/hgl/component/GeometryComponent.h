#pragma once

#include<hgl/component/SceneComponent.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/OBB.h>

COMPONENT_NAMESPACE_BEGIN

/**
* 几何组件<br>
* 组件中的元素必须是一个可以明确描述的几何体，可以被明确标记尺寸、参与空间、物理计算等。
*/
class GeometryComponent:public SceneComponent
{
public:

    using SceneComponent::SceneComponent;

    virtual ~GeometryComponent()=default;

    virtual const bool GetLocalAABB(AABB &box) const=0;

    const bool GetWorldAABB(AABB &box);
    const bool GetWorldOBB(OBB &box);
    const bool GetWorldOBBMatrix(Matrix4f &obb_matrix,const float cube_size=1.0f);
};//class GeometryComponent

COMPONENT_NAMESPACE_END
