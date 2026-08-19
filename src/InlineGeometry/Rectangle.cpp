#include "InlineGeometryCommon.h"
#include<hgl/graph/geo/GeometryBuilder.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateRectangle(GeometryCreater *pc,const RectangleCreateInfo *rci)
    {
        // 1. 参数验证
        if(!pc || !rci)
            return nullptr;

        if(rci->segments_x < 1 || rci->segments_y < 1)
            return nullptr;

        // 2. 计算顶点和索引数量
        uint vertex_count = (rci->segments_x + 1) * (rci->segments_y + 1);
        uint index_count = rci->segments_x * rci->segments_y * 6;

        // 3. 初始化 GeometryCreater
        if(!pc->Init("Rectangle", vertex_count, index_count))
            return nullptr;

        // 4. 初始化 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 5. 获取矩形范围
        float left = rci->scope.Left;
        float top = rci->scope.Top;
        float width = rci->scope.Width;
        float height = rci->scope.Height;
        float right = left + width;
        float bottom = top + height;

        // 6. 生成顶点
        for(uint y = 0; y <= rci->segments_y; ++y)
        {
            float ty = float(y) / float(rci->segments_y);
            float py = top + height * ty;

            for(uint x = 0; x <= rci->segments_x; ++x)
            {
                float tx = float(x) / float(rci->segments_x);
                float px = left + width * tx;

                // 写入顶点位置 (2D)
                builder.WriteVertex(px, py, 0.0f);

                // 写入法线 (Z+)
                if(rci->ntb == NTBType::Normal)
                    builder.WriteNTB(0.0f, 0.0f, 1.0f);

                // 写入纹理坐标
                if(rci->tex_coord)
                    builder.WriteTexCoord(tx, ty);
            }
        }

        // 7. 生成索引
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U8)
        {
            auto idx_accessor = pc->GetIndexAccessor<uint8_t>();
            auto *idx8 = idx_accessor.Get() ? idx_accessor.Get()->Get() : nullptr;
            if(!idx8)
                return nullptr;

            for(uint y = 0; y < rci->segments_y; ++y)
            {
                for(uint x = 0; x < rci->segments_x; ++x)
                {
                    uint i0 = y * (rci->segments_x + 1) + x;
                    uint i1 = i0 + 1;
                    uint i2 = i0 + (rci->segments_x + 1);
                    uint i3 = i2 + 1;

                    // 第一个三角形
                    *idx8++ = (uint8_t)i0;
                    *idx8++ = (uint8_t)i2;
                    *idx8++ = (uint8_t)i1;

                    // 第二个三角形
                    *idx8++ = (uint8_t)i1;
                    *idx8++ = (uint8_t)i2;
                    *idx8++ = (uint8_t)i3;
                }
            }
        }
        else if(index_type == IndexType::U16)
        {
            auto idx_accessor = pc->GetIndexAccessor<uint16_t>();
            auto *idx16 = idx_accessor.Get() ? idx_accessor.Get()->Get() : nullptr;
            if(!idx16)
                return nullptr;

            for(uint y = 0; y < rci->segments_y; ++y)
            {
                for(uint x = 0; x < rci->segments_x; ++x)
                {
                    uint i0 = y * (rci->segments_x + 1) + x;
                    uint i1 = i0 + 1;
                    uint i2 = i0 + (rci->segments_x + 1);
                    uint i3 = i2 + 1;

                    // 第一个三角形
                    *idx16++ = (uint16_t)i0;
                    *idx16++ = (uint16_t)i2;
                    *idx16++ = (uint16_t)i1;

                    // 第二个三角形
                    *idx16++ = (uint16_t)i1;
                    *idx16++ = (uint16_t)i2;
                    *idx16++ = (uint16_t)i3;
                }
            }
        }
        else if(index_type == IndexType::U32)
        {
            auto idx_accessor = pc->GetIndexAccessor<uint32_t>();
            auto *idx32 = idx_accessor.Get() ? idx_accessor.Get()->Get() : nullptr;
            if(!idx32)
                return nullptr;

            for(uint y = 0; y < rci->segments_y; ++y)
            {
                for(uint x = 0; x < rci->segments_x; ++x)
                {
                    uint i0 = y * (rci->segments_x + 1) + x;
                    uint i1 = i0 + 1;
                    uint i2 = i0 + (rci->segments_x + 1);
                    uint i3 = i2 + 1;

                    // 第一个三角形
                    *idx32++ = i0;
                    *idx32++ = i2;
                    *idx32++ = i1;

                    // 第二个三角形
                    *idx32++ = i1;
                    *idx32++ = i2;
                    *idx32++ = i3;
                }
            }
        }
        else
        {
            return nullptr;
        }

        // 8. 创建几何体并设置包围盒
        return pc->CreateWithAABB(
            Vector3f(left, top, -0.01f),
            Vector3f(right, bottom, 0.01f));
    }

} // namespace
