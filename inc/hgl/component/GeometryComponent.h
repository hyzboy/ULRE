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

    const bool GetWorldOBB(OBB &box)
    {
        AABB aabb;

        if(!GetLocalAABB(aabb))
            return false;

        box.Set(GetLocalToWorldMatrix(),aabb);

        return true;
    }

    const bool GetWorldOBBMatrix(Matrix4f &obb_matrix,const float cube_size=1.0f)
    {
        AABB aabb;

        if(!GetLocalAABB(aabb))
            return false;

        //这两行也是对的
        //OBB obb(GetLocalToWorldMatrix(),aabb);
        //obb_matrix=obb.GetMatrix(cube_size);

        // 1. 计算最终的缩放向量
        const Vector3f scale_vector = aabb.GetLength() * cube_size;

        // 2. 直接构建局部空间的变换矩阵 (Translate * Scale)
        Matrix4f local_aabb_matrix;

        local_aabb_matrix[0] = Vector4f(scale_vector.x, 0.0f, 0.0f, 0.0f);
        local_aabb_matrix[1] = Vector4f(0.0f, scale_vector.y, 0.0f, 0.0f);
        local_aabb_matrix[2] = Vector4f(0.0f, 0.0f, scale_vector.z, 0.0f);
        local_aabb_matrix[3] = Vector4f(aabb.GetCenter(), 1.0f);

        // 3. 将局部 AABB 矩阵与组件的 LocalToWorld 矩阵相乘，得到最终的世界 OBB 矩阵
        obb_matrix = GetLocalToWorldMatrix() * local_aabb_matrix;

        return(true);
    }
};//class GeometryComponent

COMPONENT_NAMESPACE_END
