#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCone(GeometryCreater *pc, const ConeCreateInfo *cci)
    {
        // 1. 参数验证
        if(!pc || !cci)
            return nullptr;

        // 2. 验证参数合法性
        if(!GeometryValidator::ValidateSlicesAndStacks(cci->numberSlices, cci->numberStacks))
            return nullptr;

        // 3. 计算顶点和索引数量
        const uint numberVertices = (cci->numberSlices + 2) + (cci->numberSlices + 1) * (cci->numberStacks + 1);
        const uint numberIndices = cci->numberSlices * 3 + cci->numberSlices * 6 * cci->numberStacks;

        // 4. 基本验证
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        // 5. 初始化
        if(!pc->Init("Cone", numberVertices, numberIndices))
            return nullptr;

        // 6. 创建 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 7. 预计算常量
        const float angleStep = (2.0f * std::numbers::pi_v<float>) / float(cci->numberSlices);
        const float h = 2.0f * cci->halfExtend;
        const float r = cci->radius;
        const float l = sqrtf(h * h + r * r);

        // 8. 生成底面中心顶点
        builder.WriteFullVertex(
            0.0f, 0.0f, -cci->halfExtend,
            0.0f, 0.0f, -1.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f
        );

        // 9. 生成底面圆周顶点
        for(uint i = 0; i <= cci->numberSlices; ++i)
        {
            const float currentAngle = angleStep * float(i);
            const float cosAngle = cos(currentAngle);
            const float sinAngle = sin(currentAngle);

            builder.WriteFullVertex(
                cosAngle * cci->radius,
                -sinAngle * cci->radius,
                -cci->halfExtend,
                0.0f, 0.0f, -1.0f,
                sinAngle, cosAngle, 0.0f,
                0.0f, 0.0f
            );
        }

        // 10. 生成侧面顶点（从底部到顶点）
        for(uint j = 0; j <= cci->numberStacks; ++j)
        {
            const float level = float(j) / float(cci->numberStacks);
            const float currentRadius = cci->radius * (1.0f - level);
            const float z = -cci->halfExtend + 2.0f * cci->halfExtend * level;

            for(uint i = 0; i <= cci->numberSlices; ++i)
            {
                const float currentAngle = angleStep * float(i);
                const float cosAngle = cos(currentAngle);
                const float sinAngle = sin(currentAngle);

                // 位置
                const float x = cosAngle * currentRadius;
                const float y = -sinAngle * currentRadius;

                // 法线（圆锥侧面法线）
                const float nx = (h / l) * cosAngle;
                const float ny = -(h / l) * sinAngle;
                const float nz = r / l;

                // 切线
                const float tx = -sinAngle;
                const float ty = -cosAngle;
                const float tz = 0.0f;

                // 纹理坐标
                const float u = float(i) / float(cci->numberSlices);
                const float v = level;

                builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, u, v);
            }
        }

        // 11. 生成索引
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
            IndexGenerator::CreateConeIndices<uint16>(pc, cci->numberSlices, cci->numberStacks);
        else if(index_type == IndexType::U32)
            IndexGenerator::CreateConeIndices<uint32>(pc, cci->numberSlices, cci->numberStacks);
        else if(index_type == IndexType::U8)
            IndexGenerator::CreateConeIndices<uint8>(pc, cci->numberSlices, cci->numberStacks);
        else
            return nullptr;

        // 12. 创建几何体并设置包围体
        return pc->CreateWithAABB(
            Vector3f(-cci->radius, -cci->radius, -cci->halfExtend),
            Vector3f(cci->radius, cci->radius, cci->halfExtend)
        );
    }
}//namespace hgl::graph::inline_geometry
