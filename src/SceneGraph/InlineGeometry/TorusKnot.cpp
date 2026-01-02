#include"InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateTorusKnot(GeometryCreater *pc, const TorusKnotCreateInfo *tkci)
    {
        if(!pc||!tkci)return(nullptr);

        // 基本参数验证
        if(tkci->major_radius <= 0.0f || tkci->minor_radius <= 0.0f)
            return nullptr;
        
        if(tkci->p < 1 || tkci->q < 1)
            return nullptr;
            
        if(tkci->major_segments < 3 || tkci->minor_segments < 3)
            return nullptr;

        const uint total_vertices = (tkci->major_segments + 1) * (tkci->minor_segments + 1);
        const uint total_indices = tkci->major_segments * tkci->minor_segments * 6;

        // 验证顶点和索引数量
        if(!GeometryValidator::ValidateBasicParams(pc, total_vertices, total_indices))
            return nullptr;

        if(!pc->Init("TorusKnot", total_vertices, total_indices))
            return nullptr;

        GeometryBuilder builder(pc);
        if(!builder.IsValid())return nullptr;

        const float R = tkci->major_radius;
        const float r = tkci->minor_radius;
        const uint p = tkci->p;
        const uint q = tkci->q;
        const uint nU = tkci->major_segments;
        const uint nV = tkci->minor_segments;

        // 生成顶点
        for(uint i = 0; i <= nU; ++i)
        {
            const float u = 2.0f * std::numbers::pi_v<float> * float(i) / float(nU);
            
            // 环面纽结路径参数方程
            const float cosp_u = cos(p * u);
            const float sinp_u = sin(p * u);
            const float cosq_u = cos(q * u);
            const float sinq_u = sin(q * u);
            
            // 计算路径位置
            float path_x = R * (2.0f + cosp_u) * cosq_u;
            float path_y = R * (2.0f + cosp_u) * sinq_u;
            float path_z = R * sinp_u;
            
            // 计算切向量
            float tang_x = -R * p * sinp_u * cosq_u - R * q * (2.0f + cosp_u) * sinq_u;
            float tang_y = -R * p * sinp_u * sinq_u + R * q * (2.0f + cosp_u) * cosq_u;
            float tang_z = R * p * cosp_u;
            
            // 归一化切向量
            float tang_len = sqrtf(tang_x * tang_x + tang_y * tang_y + tang_z * tang_z);
            if(tang_len > 0.0001f)
            {
                tang_x /= tang_len;
                tang_y /= tang_len;
                tang_z /= tang_len;
            }
            
            // 计算径向向量（简化版本：使用Y轴）
            float radial_x = 0.0f;
            float radial_y = 1.0f;
            float radial_z = 0.0f;
            
            // 计算副法向量 (binormal = tangent × radial)
            float binorm_x = tang_y * radial_z - tang_z * radial_y;
            float binorm_y = tang_z * radial_x - tang_x * radial_z;
            float binorm_z = tang_x * radial_y - tang_y * radial_x;
            
            // 归一化副法向量
            float binorm_len = sqrtf(binorm_x * binorm_x + binorm_y * binorm_y + binorm_z * binorm_z);
            if(binorm_len > 0.0001f)
            {
                binorm_x /= binorm_len;
                binorm_y /= binorm_len;
                binorm_z /= binorm_len;
            }
            
            // 计算法向量 (normal = binormal × tangent)
            float norm_base_x = binorm_y * tang_z - binorm_z * tang_y;
            float norm_base_y = binorm_z * tang_x - binorm_x * tang_z;
            float norm_base_z = binorm_x * tang_y - binorm_y * tang_x;
            
            // 沿管道圆周生成顶点
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * std::numbers::pi_v<float> * float(j) / float(nV);
                const float cos_v = cos(v);
                const float sin_v = sin(v);
                
                // 计算截面位置
                float offset_x = r * (cos_v * norm_base_x + sin_v * binorm_x);
                float offset_y = r * (cos_v * norm_base_y + sin_v * binorm_y);
                float offset_z = r * (cos_v * norm_base_z + sin_v * binorm_z);
                
                float pos_x = path_x + offset_x;
                float pos_y = path_y + offset_y;
                float pos_z = path_z + offset_z;
                
                // 法线指向截面圆心的外侧
                float normal_x = cos_v * norm_base_x + sin_v * binorm_x;
                float normal_y = cos_v * norm_base_y + sin_v * binorm_y;
                float normal_z = cos_v * norm_base_z + sin_v * binorm_z;
                
                // 归一化法线
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                const float tex_u = float(i) / float(nU);
                const float tex_v = float(j) / float(nV);

                if(builder.HasTangents())
                {
                    builder.WriteFullVertex(pos_x, pos_y, pos_z,
                                          normal_x, normal_y, normal_z,
                                          tang_x, tang_y, tang_z,
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

        Geometry *geo = pc->Create();
        if(!geo)return nullptr;

        // 计算包围盒
        const float max_extent = R * 3.0f + r;
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-max_extent, -max_extent, -R - r),
                      Vector3f(max_extent, max_extent, R + r));
        geo->SetBoundingVolumes(bv);

        return geo;
    }
}//namespace hgl::graph::inline_geometry
