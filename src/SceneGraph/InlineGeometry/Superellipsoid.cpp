#include"InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    // 带符号的幂函数，保留符号
    static inline float signed_pow(float value, float exponent)
    {
        if(value >= 0.0f)
            return powf(value, exponent);
        else
            return -powf(-value, exponent);
    }

    Geometry *CreateSuperellipsoid(GeometryCreater *pc, const SuperellipsoidCreateInfo *seci)
    {
        if(!pc||!seci)return(nullptr);

        // 基本参数验证
        if(seci->size.x <= 0.0f || seci->size.y <= 0.0f || seci->size.z <= 0.0f)
            return nullptr;
        
        if(seci->n1 <= 0.0f || seci->n2 <= 0.0f)
            return nullptr;
            
        if(seci->u_segments < 3 || seci->v_segments < 2)
            return nullptr;

        const uint nU = seci->u_segments;
        const uint nV = seci->v_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        // 验证顶点和索引数量
        if(!GeometryValidator::ValidateBasicParams(pc, total_vertices, total_indices))
            return nullptr;

        if(!pc->Init("Superellipsoid", total_vertices, total_indices))
            return nullptr;

        GeometryBuilder builder(pc);
        if(!builder.IsValid())return nullptr;

        const float size_x = seci->size.x;
        const float size_y = seci->size.y;
        const float size_z = seci->size.z;
        const float n1 = seci->n1;
        const float n2 = seci->n2;

        // 生成顶点 - 超椭球参数方程
        for(uint j = 0; j <= nV; ++j)
        {
            const float v = -std::numbers::pi_v<float> * 0.5f + std::numbers::pi_v<float> * float(j) / float(nV);  // -π/2 到 π/2
            const float cos_v = cos(v);
            const float sin_v = sin(v);
            
            // 超椭球形状函数
            const float cos_v_n2 = signed_pow(fabsf(cos_v), 2.0f / n2);
            const float sin_v_n2 = signed_pow(fabsf(sin_v), 2.0f / n2);
            const float z_factor = sin_v_n2 * (sin_v >= 0.0f ? 1.0f : -1.0f);
            
            for(uint i = 0; i <= nU; ++i)
            {
                const float u = 2.0f * std::numbers::pi_v<float> * float(i) / float(nU);
                const float cos_u = cos(u);
                const float sin_u = sin(u);
                
                // 超椭球形状函数
                const float cos_u_n1 = signed_pow(fabsf(cos_u), 2.0f / n1);
                const float sin_u_n1 = signed_pow(fabsf(sin_u), 2.0f / n1);
                
                const float x_factor = cos_v_n2 * cos_u_n1 * (cos_u >= 0.0f ? 1.0f : -1.0f);
                const float y_factor = cos_v_n2 * sin_u_n1 * (sin_u >= 0.0f ? 1.0f : -1.0f);
                
                // 计算位置
                float pos_x = size_x * x_factor;
                float pos_y = size_y * y_factor;
                float pos_z = size_z * z_factor;
                
                // 计算数值法线
                const float delta = 0.001f;
                
                // du方向
                const float u_next = u + delta;
                const float cos_u_next = cos(u_next);
                const float sin_u_next = sin(u_next);
                const float cos_u_next_n1 = signed_pow(fabsf(cos_u_next), 2.0f / n1);
                const float sin_u_next_n1 = signed_pow(fabsf(sin_u_next), 2.0f / n1);
                
                float pos_u_plus_x = size_x * cos_v_n2 * cos_u_next_n1 * (cos_u_next >= 0.0f ? 1.0f : -1.0f);
                float pos_u_plus_y = size_y * cos_v_n2 * sin_u_next_n1 * (sin_u_next >= 0.0f ? 1.0f : -1.0f);
                float pos_u_plus_z = pos_z;
                
                // dv方向
                const float v_next = v + delta;
                const float cos_v_next = cos(v_next);
                const float sin_v_next = sin(v_next);
                const float cos_v_next_n2 = signed_pow(fabsf(cos_v_next), 2.0f / n2);
                const float sin_v_next_n2 = signed_pow(fabsf(sin_v_next), 2.0f / n2);
                
                float pos_v_plus_x = size_x * cos_v_next_n2 * cos_u_n1 * (cos_u >= 0.0f ? 1.0f : -1.0f);
                float pos_v_plus_y = size_y * cos_v_next_n2 * sin_u_n1 * (sin_u >= 0.0f ? 1.0f : -1.0f);
                float pos_v_plus_z = size_z * sin_v_next_n2 * (sin_v_next >= 0.0f ? 1.0f : -1.0f);
                
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

        // 生成索引 (使用Sphere索引模式)
        {
            const IndexType index_type = pc->GetIndexType();

            if(index_type == IndexType::U16)
                IndexGenerator::CreateSphereIndices<uint16>(pc, nV, nU);
            else if(index_type == IndexType::U32)
                IndexGenerator::CreateSphereIndices<uint32>(pc, nV, nU);
            else if(index_type == IndexType::U8)
                IndexGenerator::CreateSphereIndices<uint8>(pc, nV, nU);
            else
                return nullptr;
        }

        Geometry *p = pc->Create();
        if(!p)return nullptr;

        // 计算包围盒
        Vector3f size(size_x, size_y, size_z);
        BoundingVolumes bv;
        bv.SetFromAABB(-size, size);
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
