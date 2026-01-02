#include"InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateMobiusStrip(GeometryCreater *pc, const MobiusStripCreateInfo *msci)
    {
        if(!pc||!msci)return(nullptr);

        // 基本参数验证
        if(msci->radius <= 0.0f || msci->width <= 0.0f)
            return nullptr;
        
        if(msci->twists < 1)
            return nullptr;
            
        if(msci->length_segments < 3 || msci->width_segments < 1)
            return nullptr;

        const uint nU = msci->length_segments;
        const uint nV = msci->width_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        // 验证顶点和索引数量
        if(!GeometryValidator::ValidateBasicParams(pc, total_vertices, total_indices))
            return nullptr;

        if(!pc->Init("MobiusStrip", total_vertices, total_indices))
            return nullptr;

        GeometryBuilder builder(pc);
        if(!builder.IsValid())return nullptr;

        const float R = msci->radius;
        const float w = msci->width * 0.5f;  // 半宽
        const uint twists = msci->twists;

        // 生成顶点
        for(uint i = 0; i <= nU; ++i)
        {
            const float u = 2.0f * std::numbers::pi_v<float> * float(i) / float(nU);
            const float twist_angle = twists * u * 0.5f;  // 扭转角度
            
            const float cos_u = cos(u);
            const float sin_u = sin(u);
            const float cos_twist = cos(twist_angle);
            const float sin_twist = sin(twist_angle);
            
            // 路径中心点
            float center_x = R * cos_u;
            float center_y = R * sin_u;
            float center_z = 0.0f;
            
            // 切向量
            float tangent_x = -sin_u;
            float tangent_y = cos_u;
            float tangent_z = 0.0f;
            
            // 法向量（随扭转旋转）
            float rotated_normal_x = 0.0f;
            float rotated_normal_y = -sin_twist;
            float rotated_normal_z = cos_twist;
            
            // 副法向量（宽度方向）
            float binormal_x = cos_twist * cos_u;
            float binormal_y = cos_twist * sin_u;
            float binormal_z = sin_twist;
            
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * w * (float(j) / float(nV) - 0.5f);
                
                // 计算位置
                float pos_x = center_x + v * binormal_x;
                float pos_y = center_y + v * binormal_y;
                float pos_z = center_z + v * binormal_z;
                
                const float tex_u = float(i) / float(nU);
                const float tex_v = float(j) / float(nV);

                if(builder.HasTangents())
                {
                    builder.WriteFullVertex(pos_x, pos_y, pos_z,
                                          rotated_normal_x, rotated_normal_y, rotated_normal_z,
                                          tangent_x, tangent_y, tangent_z,
                                          tex_u, tex_v);
                }
                else
                {
                    builder.WriteVertex(pos_x, pos_y, pos_z);
                    builder.WriteNormal(rotated_normal_x, rotated_normal_y, rotated_normal_z);
                    builder.WriteTexCoord(tex_u, tex_v);
                }
            }
        }

        // 生成索引 (使用Torus索引模式)
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

        const float max_extent = R + w;
        
        return pc->CreateWithAABB(
            Vector3f(-max_extent, -max_extent, -w),
            Vector3f(max_extent, max_extent, w));
    }
}//namespace hgl::graph::inline_geometry
