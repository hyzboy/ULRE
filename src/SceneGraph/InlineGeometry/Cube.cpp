#include "InlineGeometryCommon.h"
#include<hgl/graph/geo/GeometryBuilder.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci)
    {
        // 1. 参数验证
        if(!pc || !cci)
            return nullptr;
            
        if(cci->segments_x < 1 || cci->segments_y < 1 || cci->segments_z < 1)
            return nullptr;

        // 2. 计算顶点和索引数量
        auto calc_face_verts = [](uint seg_u, uint seg_v) { 
            return (seg_u + 1) * (seg_v + 1); 
        };
        auto calc_face_indices = [](uint seg_u, uint seg_v) { 
            return seg_u * seg_v * 6; 
        };
        
        uint vertex_count = 0;
        uint index_count = 0;
        
        // 顶面和底面 (XZ平面)
        vertex_count += calc_face_verts(cci->segments_x, cci->segments_z) * 2;
        index_count += calc_face_indices(cci->segments_x, cci->segments_z) * 2;
        
        // 前面和后面 (XY平面)
        vertex_count += calc_face_verts(cci->segments_x, cci->segments_y) * 2;
        index_count += calc_face_indices(cci->segments_x, cci->segments_y) * 2;
        
        // 左面和右面 (ZY平面)
        vertex_count += calc_face_verts(cci->segments_z, cci->segments_y) * 2;
        index_count += calc_face_indices(cci->segments_z, cci->segments_y) * 2;

        // 3. 初始化 GeometryCreater
        if(!pc->Init("Cube", vertex_count, index_count))
            return nullptr;

        // 4. 初始化 GeometryBuilder 获取VAB映射
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 5. 获取索引缓冲区映射
        IBMap *ib_map = pc->GetIBMap();
        if(!ib_map)
            return nullptr;
        
        // 根据索引类型获取索引写入指针
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

        // 辅助函数：写入索引
        auto write_index = [&](uint idx) {
            if(idx8)        *idx8++  = (uint8_t)idx;
            else if(idx16)  *idx16++ = (uint16_t)idx;
            else if(idx32)  *idx32++ = idx;
        };

        // 6. 生成6个面的顶点和索引
        const float half = 0.5f;
        uint base_vertex = 0;

        // 辅助函数：生成一个细分面
        auto generate_face = [&](
            const Vector3f& origin,
            const Vector3f& u_axis,
            const Vector3f& v_axis,
            const Vector3f& normal,
            const Vector3f& tangent,
            uint segments_u,
            uint segments_v)
        {
            // 生成顶点
            for(uint v = 0; v <= segments_v; ++v)
            {
                float tv = float(v) / float(segments_v);
                
                for(uint u = 0; u <= segments_u; ++u)
                {
                    float tu = float(u) / float(segments_u);
                    
                    Vector3f pos = origin + u_axis * tu + v_axis * tv;
                    
                    builder.WriteVertex(pos.x, pos.y, pos.z);
                    
                    if(cci->normal)
                        builder.WriteNormal(normal.x, normal.y, normal.z);
                    
                    if(cci->tangent)
                        builder.WriteTangent(tangent.x, tangent.y, tangent.z);
                    
                    if(cci->tex_coord)
                        builder.WriteTexCoord(tu, tv);
                }
            }
            
            // 生成索引
            for(uint v = 0; v < segments_v; ++v)
            {
                for(uint u = 0; u < segments_u; ++u)
                {
                    uint i0 = base_vertex + v * (segments_u + 1) + u;
                    uint i1 = i0 + 1;
                    uint i2 = i0 + (segments_u + 1);
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
            
            base_vertex += (segments_u + 1) * (segments_v + 1);
        };

        // 底面 (Y-)
        generate_face(
            Vector3f(-half, -half, -half),
            Vector3f(1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 0.0f, 1.0f),
            Vector3f(0.0f, -1.0f, 0.0f),
            Vector3f(1.0f, 0.0f, 0.0f),
            cci->segments_x, cci->segments_z);
        
        // 顶面 (Y+)
        generate_face(
            Vector3f(-half, half, half),
            Vector3f(1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 0.0f, -1.0f),
            Vector3f(0.0f, 1.0f, 0.0f),
            Vector3f(1.0f, 0.0f, 0.0f),
            cci->segments_x, cci->segments_z);
        
        // 后面 (Z-)
        generate_face(
            Vector3f(half, -half, -half),
            Vector3f(-1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 1.0f, 0.0f),
            Vector3f(0.0f, 0.0f, -1.0f),
            Vector3f(-1.0f, 0.0f, 0.0f),
            cci->segments_x, cci->segments_y);
        
        // 前面 (Z+)
        generate_face(
            Vector3f(-half, -half, half),
            Vector3f(1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 1.0f, 0.0f),
            Vector3f(0.0f, 0.0f, 1.0f),
            Vector3f(1.0f, 0.0f, 0.0f),
            cci->segments_x, cci->segments_y);
        
        // 左面 (X-)
        generate_face(
            Vector3f(-half, -half, half),
            Vector3f(0.0f, 0.0f, -1.0f),
            Vector3f(0.0f, 1.0f, 0.0f),
            Vector3f(-1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 0.0f, -1.0f),
            cci->segments_z, cci->segments_y);
        
        // 右面 (X+)
        generate_face(
            Vector3f(half, -half, -half),
            Vector3f(0.0f, 0.0f, 1.0f),
            Vector3f(0.0f, 1.0f, 0.0f),
            Vector3f(1.0f, 0.0f, 0.0f),
            Vector3f(0.0f, 0.0f, 1.0f),
            cci->segments_z, cci->segments_y);

        // 7. Unmap索引缓冲区
        ib_map->Unmap();

        // 8. 创建几何体并设置包围体
        return pc->CreateWithAABB(
            Vector3f(-0.5f, -0.5f, -0.5f),
            Vector3f(0.5f, 0.5f, 0.5f));
    }
}//namespace hgl::graph::inline_geometry
