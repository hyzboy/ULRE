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
                if(rci->normal)
                    builder.WriteNormal(0.0f, 0.0f, 1.0f);
                
                // 写入纹理坐标
                if(rci->tex_coord)
                    builder.WriteTexCoord(tx, ty);
            }
        }

        // 7. 生成索引
        IBMap *ib_map = pc->GetIBMap();
        if(!ib_map)
            return nullptr;
        
        const IndexType index_type = pc->GetIndexType();
        uint8_t  *idx8  = nullptr;
        uint16_t *idx16 = nullptr;
        uint32_t *idx32 = nullptr;
        
        if(index_type == IndexType::U8)
            idx8 = (uint8_t*)ib_map->Map();
        else if(index_type == IndexType::U16)
            idx16 = (uint16_t*)ib_map->Map();
        else if(index_type == IndexType::U32)
            idx32 = (uint32_t*)ib_map->Map();
        else
            return nullptr;

        auto write_index = [&](uint idx) {
            if(idx8)        *idx8++  = (uint8_t)idx;
            else if(idx16)  *idx16++ = (uint16_t)idx;
            else if(idx32)  *idx32++ = idx;
        };

        for(uint y = 0; y < rci->segments_y; ++y)
        {
            for(uint x = 0; x < rci->segments_x; ++x)
            {
                uint i0 = y * (rci->segments_x + 1) + x;
                uint i1 = i0 + 1;
                uint i2 = i0 + (rci->segments_x + 1);
                uint i3 = i2 + 1;
                
                // 第一个三角形
                write_index(i0);
                write_index(i2);
                write_index(i1);
                
                // 第二个三角形
                write_index(i1);
                write_index(i2);
                write_index(i3);
            }
        }

        ib_map->Unmap();

        // 8. 创建几何体并设置包围盒
        return pc->CreateWithAABB(
            Vector3f(left, top, -0.01f),
            Vector3f(right, bottom, 0.01f));
    }

    Geometry *CreateGBufferCompositionRectangle(GeometryCreater *pc)
    {
        RectangleCreateInfo rci;

        rci.scope.Set(-1,-1,2,2);

        return CreateRectangle(pc,&rci);
    }
} // namespace
