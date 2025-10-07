#include<hgl/component/GeometryComponent.h>
#include<hgl/graph/BoundingVolumes.h>

COMPONENT_NAMESPACE_BEGIN
const bool GeometryComponent::GetWorldAABB(AABB &box)
{
    AABB local_aabb;

    if(!GetLocalAABB(local_aabb))
        return false;

    box = local_aabb.Transformed(GetLocalToWorldMatrix());
    return true;
}

const bool GeometryComponent::GetWorldOBB(OBB &box)
{
    AABB local_aabb;

    if(!GetLocalAABB(local_aabb))
        return false;

    box = ToOBB(local_aabb, GetLocalToWorldMatrix());
    return true;
}

const bool GeometryComponent::GetWorldOBBMatrix(Matrix4f &obb_matrix,const float cube_size)
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

    local_aabb_matrix[0] = Vector4f(scale_vector.x,0.0f,0.0f,0.0f);
    local_aabb_matrix[1] = Vector4f(0.0f,scale_vector.y,0.0f,0.0f);
    local_aabb_matrix[2] = Vector4f(0.0f,0.0f,scale_vector.z,0.0f);
    local_aabb_matrix[3] = Vector4f(aabb.GetCenter(),1.0f);

    // 3. 将局部 AABB 矩阵与组件的 LocalToWorld 矩阵相乘，得到最终的世界 OBB 矩阵
    obb_matrix = GetLocalToWorldMatrix() * local_aabb_matrix;

    return(true);
}
COMPONENT_NAMESPACE_END
