#include<hgl/component/GeometryComponent.h>

COMPONENT_NAMESPACE_BEGIN
const bool GeometryComponent::GetWorldAABB(AABB &box)
{
    AABB aabb;

    if(!GetLocalAABB(aabb))
        return false;

    const Matrix4f &l2w = GetLocalToWorldMatrix();

    // Compute world AABB by transforming the local AABB via the L2W matrix.
    // Efficient method: transform center and compute world half-extents using absolute of rotation part.

    // Local center and half-length
    const Vector3f localCenter = aabb.GetCenter();
    const Vector3f localHalf = aabb.GetLength() * 0.5f;

    // Transform center (apply full 4x4 transform)
    Vector4f c4(localCenter,1.0f);
    Vector4f wc4 = l2w * c4;
    Vector3f worldCenter = Vector3f(wc4.x,wc4.y,wc4.z);

    // Extract upper-left 3x3 matrix (rotation + scale)
    Matrix3f m3;
    m3[0] = Vector3f(l2w[0].x,l2w[0].y,l2w[0].z);
    m3[1] = Vector3f(l2w[1].x,l2w[1].y,l2w[1].z);
    m3[2] = Vector3f(l2w[2].x,l2w[2].y,l2w[2].z);

    // Compute world half extents as absolute(m3) * localHalf
    Matrix3f absm3;
    for(int r = 0;r < 3;++r)
    {
        for(int c = 0;c < 3;++c)
        {
            absm3[r][c] = std::abs(m3[r][c]);
        }
    }

    Vector3f worldHalf;
    worldHalf.x = absm3[0][0] * localHalf.x + absm3[0][1] * localHalf.y + absm3[0][2] * localHalf.z;
    worldHalf.y = absm3[1][0] * localHalf.x + absm3[1][1] * localHalf.y + absm3[1][2] * localHalf.z;
    worldHalf.z = absm3[2][0] * localHalf.x + absm3[2][1] * localHalf.y + absm3[2][2] * localHalf.z;

    // Build min/max
    Vector3f minp = worldCenter - worldHalf;
    Vector3f maxp = worldCenter + worldHalf;

    box.SetMinMax(minp,maxp);
    return true;
}

const bool GeometryComponent::GetWorldOBB(OBB &box)
{
    AABB aabb;

    if(!GetLocalAABB(aabb))
        return false;

    box.Set(GetLocalToWorldMatrix(),aabb);

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
