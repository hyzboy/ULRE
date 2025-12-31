#include"InlineGeometryCommon.h"
#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateKleinBottle(GeometryCreater *pc, const KleinBottleCreateInfo *kbci)
    {
        if(!pc||!kbci)return(nullptr);

        // 基本参数验证
        if(kbci->major_radius <= 0.0f || kbci->minor_radius <= 0.0f)
            return nullptr;
            
        if(kbci->u_segments < 3 || kbci->v_segments < 3)
            return nullptr;

        const uint nU = kbci->u_segments;
        const uint nV = kbci->v_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        // 验证顶点和索引数量
        if(!GeometryValidator::ValidateBasicParams(pc, total_vertices, total_indices))
            return nullptr;

        if(!pc->Init("KleinBottle", total_vertices, total_indices))
            return nullptr;

        GeometryBuilder builder(pc);
        if(!builder.IsValid())return nullptr;

        const float R = kbci->major_radius;
        const float r = kbci->minor_radius;

        // 生成顶点 - 使用克莱因瓶的参数方程（figure-8沉浸）
        for(uint i = 0; i <= nU; ++i)
        {
            const float u = 2.0f * math::pi * float(i) / float(nU);
            const float cos_u = cos(u);
            const float sin_u = sin(u);
            
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * math::pi * float(j) / float(nV);
                const float cos_v = cos(v);
                const float sin_v = sin(v);
                
                // 克莱因瓶参数方程（改进的figure-8形式）
                float pos_x, pos_y, pos_z;
                
                if(u < math::pi)
                {
                    // 外部部分
                    pos_x = (R + r * cos_v) * cos_u;
                    pos_y = (R + r * cos_v) * sin_u;
                    pos_z = r * sin_v;
                }
                else
                {
                    // 穿过自身的部分
                    pos_x = (R + r * cos_v) * cos_u;
                    pos_y = (R + r * cos_v) * sin_u;
                    pos_z = r * sin_v;
                }
                
                // 添加figure-8扭转
                const float twist = sin(u);
                pos_x += twist * r * 0.5f * sin_v * cos_u;
                pos_y += twist * r * 0.5f * sin_v * sin_u;
                
                // 计算数值法线
                const float delta = 0.01f;
                
                float pos_u_plus_x, pos_u_plus_y, pos_u_plus_z;
                float pos_v_plus_x, pos_v_plus_y, pos_v_plus_z;
                
                float u_next = u + delta;
                float v_next = v + delta;
                
                // du方向
                float cos_u_next = cos(u_next);
                float sin_u_next = sin(u_next);
                float twist_next = sin(u_next);
                
                if(u_next < math::pi)
                {
                    pos_u_plus_x = (R + r * cos_v) * cos_u_next + twist_next * r * 0.5f * sin_v * cos_u_next;
                    pos_u_plus_y = (R + r * cos_v) * sin_u_next + twist_next * r * 0.5f * sin_v * sin_u_next;
                    pos_u_plus_z = r * sin_v;
                }
                else
                {
                    pos_u_plus_x = (R + r * cos_v) * cos_u_next + twist_next * r * 0.5f * sin_v * cos_u_next;
                    pos_u_plus_y = (R + r * cos_v) * sin_u_next + twist_next * r * 0.5f * sin_v * sin_u_next;
                    pos_u_plus_z = r * sin_v;
                }
                
                // dv方向
                float cos_v_next = cos(v_next);
                float sin_v_next = sin(v_next);
                
                if(u < math::pi)
                {
                    pos_v_plus_x = (R + r * cos_v_next) * cos_u + twist * r * 0.5f * sin_v_next * cos_u;
                    pos_v_plus_y = (R + r * cos_v_next) * sin_u + twist * r * 0.5f * sin_v_next * sin_u;
                    pos_v_plus_z = r * sin_v_next;
                }
                else
                {
                    pos_v_plus_x = (R + r * cos_v_next) * cos_u + twist * r * 0.5f * sin_v_next * cos_u;
                    pos_v_plus_y = (R + r * cos_v_next) * sin_u + twist * r * 0.5f * sin_v_next * sin_u;
                    pos_v_plus_z = r * sin_v_next;
                }
                
                // 计算切向量
                float du_x = pos_u_plus_x - pos_x;
                float du_y = pos_u_plus_y - pos_y;
                float du_z = pos_u_plus_z - pos_z;
                
                float dv_x = pos_v_plus_x - pos_x;
                float dv_y = pos_v_plus_y - pos_y;
                float dv_z = pos_v_plus_z - pos_z;
                
                // 计算法线 (du × dv)
                float normal_x = du_y * dv_z - du_z * dv_y;
                float normal_y = du_z * dv_x - du_x * dv_z;
                float normal_z = du_x * dv_y - du_y * dv_x;
                
                // 归一化法线
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                // 归一化切线
                float tangent_x = du_x;
                float tangent_y = du_y;
                float tangent_z = du_z;
                float tangent_len = sqrtf(tangent_x * tangent_x + tangent_y * tangent_y + tangent_z * tangent_z);
                if(tangent_len > 0.0001f)
                {
                    tangent_x /= tangent_len;
                    tangent_y /= tangent_len;
                    tangent_z /= tangent_len;
                }
                
                const float tex_u = float(i) / float(nU);
                const float tex_v = float(j) / float(nV);

                if(builder.HasTangents())
                {
                    builder.WriteFullVertex(pos_x, pos_y, pos_z,
                                          normal_x, normal_y, normal_z,
                                          tangent_x, tangent_y, tangent_z,
                                          tex_u, tex_v);
                }
                else
                {
                    builder.WriteVertex(pos_x, pos_y, pos_z);
                    builder.WriteNormal(normal_x, normal_y, normal_z);
                    builder.WriteTexCoord(tex_u, tex_v);
                }
            }
        }

        // 生成索引 (使用Torus索引模式因为拓扑结构相同)
        {
            const IndexType index_type = pc->GetIndexType();

            if(index_type == IndexType::U16)
                IndexGenerator::CreateTorusIndices<uint16>(pc, nU, nV);
            else if(index_type == IndexType::U32)
                IndexGenerator::CreateTorusIndices<uint32>(pc, nU, nV);
            else if(index_type == IndexType::U8)
                IndexGenerator::CreateTorusIndices<uint8>(pc, nU, nV);
            else
                return nullptr;
        }

        Geometry *p = pc->Create();
        if(!p)return nullptr;

        // 计算包围盒
        const float max_extent = R + r * 1.5f;
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-max_extent, -max_extent, -r),
                      Vector3f(max_extent, max_extent, r));
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
