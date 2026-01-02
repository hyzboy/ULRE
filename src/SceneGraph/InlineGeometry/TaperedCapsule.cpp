#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateTaperedCapsule(GeometryCreater *pc,const TaperedCapsuleCreateInfo *tcci)
    {
        // 1. 参数验证
        if(!pc||!tcci)return nullptr;

        const uint slices = std::max<uint>(3, tcci->numberSlices);
        const uint stacks = std::max<uint>(1, tcci->numberStacks);
        const float bottomR = tcci->bottomRadius;
        const float topR = tcci->topRadius;
        const float halfH = tcci->halfHeight;

        // 2. 计算几何体结构
        // 构建：侧面圆台（在 -halfH 和 +halfH 之间），底部和顶部各有一个圆环
        // 顶部半球：中心在 +halfH，半径为 topR
        // 底部半球：中心在 -halfH，半径为 bottomR

        const uint side_vertices = (slices + 1) * 2; // 底环和顶环
        const uint hemi_vertices = (stacks + 1) * (slices + 1);
        const uint numberVertices = side_vertices + hemi_vertices * 2;

        const uint side_faces = slices * 2;
        const uint hemi_faces = stacks * slices * 2;
        const uint numberIndices = (side_faces + hemi_faces * 2) * 3;

        // 3. 验证参数
        if(!GeometryValidator::ValidateSlicesAndStacks(slices, stacks, 3, 1))
            return nullptr;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        // 4. 初始化
        if(!pc->Init("TaperedCapsule", numberVertices, numberIndices))
            return nullptr;

        // 5. 创建 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 6. 预计算常量
        const float angleStep = (2.0f * std::numbers::pi_v<float>) / float(slices);
        const float height = 2.0f * halfH;
        const float dr = topR - bottomR;

        // 7. 生成侧面圆台顶点（底环和顶环）
        for(uint ring=0; ring<2; ++ring)
        {
            const float z = (ring==0) ? -halfH : halfH;
            const float r = (ring==0) ? bottomR : topR;

            for(uint i=0;i<=slices;i++)
            {
                const float a = angleStep * float(i);
                const float ca = cos(a);
                const float sa = -sin(a);
                const float x = ca * r;
                const float y = sa * r;

                // 圆台侧面法线计算
                const float nx_unnorm = ca * height;
                const float ny_unnorm = sa * height;
                const float nz_unnorm = -dr;
                const float norm_len = sqrtf(nx_unnorm*nx_unnorm + ny_unnorm*ny_unnorm + nz_unnorm*nz_unnorm);
                
                const float nx = (norm_len > 0.0f) ? (nx_unnorm / norm_len) : ca;
                const float ny = (norm_len > 0.0f) ? (ny_unnorm / norm_len) : sa;
                const float nz = (norm_len > 0.0f) ? (nz_unnorm / norm_len) : 0.0f;

                // 切线沿圆周方向
                const float tx = -sa;
                const float ty = -ca;
                const float tz = 0.0f;

                // 纹理坐标
                const float u = float(i) / float(slices);
                const float v = (ring == 0) ? 0.0f : 1.0f;

                builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, u, v);
            }
        }

        // 8. 生成顶部半球顶点
        for(uint s=0;s<=stacks;s++)
        {
            const float phi = (float)s / (float)stacks * (std::numbers::pi_v<float> * 0.5f);
            const float cz = sin(phi); // 从 0 到 1
            const float r = cos(phi);  // 环半径，将被 topR 缩放

            for(uint i=0;i<=slices;i++)
            {
                const float a = angleStep * float(i);
                const float ca = cos(a);
                const float sa = -sin(a);
                
                const float x = r * ca * topR;
                const float y = r * sa * topR;
                const float z = halfH + cz * topR;

                // 法线（球面法线）
                const float nz_pre = cz * topR;
                const float norm_len = sqrtf(x*x + y*y + nz_pre*nz_pre);
                const float nx = (norm_len > 0.0f) ? (x / norm_len) : 0.0f;
                const float ny = (norm_len > 0.0f) ? (y / norm_len) : 0.0f;
                const float nz = (norm_len > 0.0f) ? (nz_pre / norm_len) : 1.0f;

                // 切线
                const float tx = -sa;
                const float ty = -ca;
                const float tz = 0.0f;

                // 纹理坐标
                const float u = float(i) / float(slices);
                const float v = 1.0f - (float)s / float(stacks);

                builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, u, v);
            }
        }

        // 9. 生成底部半球顶点
        for(uint s=0;s<=stacks;s++)
        {
            const float phi = (float)s / (float)stacks * (std::numbers::pi_v<float> * 0.5f);
            const float cz = -sin(phi); // 从 0 到 -1
            const float r = cos(phi);

            for(uint i=0;i<=slices;i++)
            {
                const float a = angleStep * float(i);
                const float ca = cos(a);
                const float sa = -sin(a);
                
                const float x = r * ca * bottomR;
                const float y = r * sa * bottomR;
                const float z = -halfH + cz * bottomR;

                // 法线（球面法线）
                const float nz_pre = cz * bottomR;
                const float norm_len = sqrtf(x*x + y*y + nz_pre*nz_pre);
                const float nx = (norm_len > 0.0f) ? (x / norm_len) : 0.0f;
                const float ny = (norm_len > 0.0f) ? (y / norm_len) : 0.0f;
                const float nz = (norm_len > 0.0f) ? (nz_pre / norm_len) : -1.0f;

                // 切线
                const float tx = -sa;
                const float ty = -ca;
                const float tz = 0.0f;

                // 纹理坐标
                const float u = float(i) / float(slices);
                const float v = (float)s / float(stacks);

                builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, u, v);
            }
        }

        // 10. 生成索引
        const IndexType index_type = pc->GetIndexType();
        const uint ringVertexCount = slices + 1;

        // 创建索引生成lambda（避免代码重复）
        auto generateIndices = [&](auto* ip) 
        {
            using IndexT = std::remove_pointer_t<decltype(ip)>;
            
            // 侧面索引
            uint base = 0;
            for(uint i=0;i<slices;i++)
            {
                const uint a = base + i;
                const uint b = base + i + 1;
                const uint c = base + ringVertexCount + i;
                const uint d = base + ringVertexCount + i + 1;

                // 保持顺时针正面
                *ip++ = (IndexT)a; *ip++ = (IndexT)d; *ip++ = (IndexT)b;
                *ip++ = (IndexT)a; *ip++ = (IndexT)c; *ip++ = (IndexT)d;
            }

            // 顶部半球索引
            const uint topBase = ringVertexCount * 2;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    const uint row1 = topBase + s * (slices + 1);
                    const uint row2 = topBase + (s + 1) * (slices + 1);

                    const uint v0 = row1 + i;
                    const uint v1 = row1 + i + 1;
                    const uint v2 = row2 + i;
                    const uint v3 = row2 + i + 1;

                    *ip++ = (IndexT)v0; *ip++ = (IndexT)v3; *ip++ = (IndexT)v1;
                    *ip++ = (IndexT)v0; *ip++ = (IndexT)v2; *ip++ = (IndexT)v3;
                }
            }

            // 底部半球索引
            const uint bottomBase = topBase + hemi_vertices;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    const uint row1 = bottomBase + s * (slices + 1);
                    const uint row2 = bottomBase + (s + 1) * (slices + 1);

                    const uint v0 = row1 + i;
                    const uint v1 = row1 + i + 1;
                    const uint v2 = row2 + i;
                    const uint v3 = row2 + i + 1;

                    *ip++ = (IndexT)v0; *ip++ = (IndexT)v1; *ip++ = (IndexT)v3;
                    *ip++ = (IndexT)v0; *ip++ = (IndexT)v3; *ip++ = (IndexT)v2;
                }
            }
        };

        if(index_type == IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            generateIndices(ib.operator uint16*());
        }
        else if(index_type == IndexType::U32)
        {
            IBTypeMap<uint32> ib(pc->GetIBMap());
            generateIndices(ib.operator uint32*());
        }
        else if(index_type == IndexType::U8)
        {
            IBTypeMap<uint8> ib(pc->GetIBMap());
            generateIndices(ib.operator uint8*());
        }
        else
            return nullptr;

        // 11. 创建几何体并设置包围体
        const float maxr = std::max(bottomR, topR);
        return pc->CreateWithAABB(
            math::Vector3f(-maxr, -maxr, -halfH - maxr),
            Vector3f(maxr, maxr, halfH + maxr)
        );
    }
}
