#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    // 创建链环：通过改编圆环创建方法，但允许椭圆形主圆
    Primitive *CreateChainLink(PrimitiveCreater *pc, const ChainLinkCreateInfo *clci)
    {
        // 1. 参数验证
        if(!pc||!clci) return nullptr;

        const uint numberSlices = (clci->numberSlices < 3) ? 3 : clci->numberSlices;
        const uint numberStacks = (clci->numberStacks < 3) ? 3 : clci->numberStacks;

        const float torusRadius = clci->minor_radius; // 截面半径
        const float centerRadius = clci->major_radius; // 中心到截面中心

        // 2. 计算顶点和索引数量
        const uint numberVertices = (numberStacks + 1) * (numberSlices + 1);
        const uint numberIndices = numberStacks * numberSlices * 2 * 3;

        // 3. 验证参数
        if(!GeometryValidator::ValidateSlicesAndStacks(numberSlices, numberStacks, 3, 3))
            return nullptr;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        // 4. 初始化
        if(!pc->Init("ChainLink", numberVertices, numberIndices))
            return nullptr;

        // 5. 创建 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 6. 预计算常量
        const float sIncr = 1.0f / float(numberSlices);
        const float tIncr = 1.0f / float(numberStacks);
        const Vector3f helpVector(0.0f, 1.0f, 0.0f);

        // 7. 生成顶点
        float s = 0.0f;
        for (uint sideCount = 0; sideCount <= numberSlices; ++sideCount, s += sIncr)
        {
            const float cosS = cos(2.0f * std::numbers::pi_v<float> * s);
            const float sinS = sin(2.0f * std::numbers::pi_v<float> * s);

            // 椭圆主圆：位置中心在 (0, centerRadius*cosS, centerRadius*sinS) 但在 Y/Z 缩放
            const float centerX = 0.0f;
            const float centerY = centerRadius * cosS * clci->scale_y;
            const float centerZ = centerRadius * sinS * clci->scale_z;

            float t = 0.0f;
            for (uint faceCount = 0; faceCount <= numberStacks; ++faceCount, t += tIncr)
            {
                const float cosT = cos(2.0f * std::numbers::pi_v<float> * t);
                const float sinT = sin(2.0f * std::numbers::pi_v<float> * t);

                // 主圆切线方向（dCenter/ds）在缩放前
                const float tangY = -centerRadius * sin(2.0f * std::numbers::pi_v<float> * s);
                const float tangZ =  centerRadius * cos(2.0f * std::numbers::pi_v<float> * s);
                // 但缩放影响切线；计算并归一化
                Vector3f majorTangent(0.0f, tangY * clci->scale_y, tangZ * clci->scale_z);
                majorTangent = glm::normalize(majorTangent);

                // 选择副法线和法线形成正交框架；因为 centerX 总是 0，使用 X 轴作为径向
                Vector3f normalDir = glm::normalize(Vector3f(1.0f, 0.0f, 0.0f));
                Vector3f binormal = glm::normalize(glm::cross(majorTangent, normalDir));
                Vector3f tangentDir = glm::normalize(glm::cross(binormal, majorTangent));

                // 次要圆上的点（在局部框架中：normalDir * (cosT*torusRadius) + tangentDir * (sinT*torusRadius)）
                Vector3f offset = normalDir * (cosT * torusRadius) + tangentDir * (sinT * torusRadius);
                Vector3f pos = Vector3f(centerX, centerY, centerZ) + offset;

                // 法线是偏移归一化后的世界空间
                Vector3f n = glm::normalize(offset);

                // 纹理坐标
                const float u = s * clci->uv_scale.x;
                const float v = t * clci->uv_scale.y;

                // 切线（使用四元数旋转）
                float tx = 0.0f, ty = 0.0f, tz = 0.0f;
                if(builder.HasTangents())
                {
                    Quatf quat = RotationQuat(360.0f * s, AxisVector::Z);
                    Matrix4f matrix = ToMatrix(quat);
                    Vector3f tangentVec = TransformDirection(matrix, helpVector);
                    tx = tangentVec.x;
                    ty = tangentVec.y;
                    tz = tangentVec.z;
                }

                builder.WriteFullVertex(pos.x, pos.y, pos.z, n.x, n.y, n.z, tx, ty, tz, u, v);
            }
        }

        // 8. 生成索引：重用圆环索引生成器
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
            IndexGenerator::CreateTorusIndices<uint16>(pc, numberSlices, numberStacks);
        else if(index_type == IndexType::U32)
            IndexGenerator::CreateTorusIndices<uint32>(pc, numberSlices, numberStacks);
        else if(index_type == IndexType::U8)
            IndexGenerator::CreateTorusIndices<uint8>(pc, numberSlices, numberStacks);
        else
            return nullptr;

        // 9. 创建几何体
        Primitive *p = pc->Create();
        
        // 10. 设置包围盒
        if(p)
        {
            // 基于缩放后的中心半径 + 环面半径保守估计包围盒
            const float maxY = fabs(centerRadius * clci->scale_y) + torusRadius;
            const float maxZ = fabs(centerRadius * clci->scale_z) + torusRadius;
            const float maxX = torusRadius;

            AABB aabb;
            aabb.SetMinMax(Vector3f(-maxX, -maxY, -maxZ), Vector3f(maxX, maxY, maxZ));
            p->SetBoundingBox(aabb);
        }

        return p;
    }

} // namespace hgl::graph::inline_geometry
