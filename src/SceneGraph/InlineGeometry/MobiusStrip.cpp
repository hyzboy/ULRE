#include"InlineGeometryCommon.h"
#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    Geometry *CreateMobiusStrip(GeometryCreater *pc, const MobiusStripCreateInfo *msci)
    {
        if(!pc||!msci)return(nullptr);

        GeometryValidator validator("MobiusStrip");
        if(!validator.CheckPositive("radius", msci->radius))return nullptr;
        if(!validator.CheckPositive("width", msci->width))return nullptr;
        if(!validator.CheckMinValue("twists", msci->twists, 1u))return nullptr;
        if(!validator.CheckMinValue("length_segments", msci->length_segments, 3u))return nullptr;
        if(!validator.CheckMinValue("width_segments", msci->width_segments, 1u))return nullptr;

        const uint nU = msci->length_segments;
        const uint nV = msci->width_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        GeometryBuilder builder(pc, total_vertices, HGL_PRIM_TRIANGLES);
        if(!builder.IsValid())return nullptr;

        const float R = msci->radius;
        const float w = msci->width * 0.5f;  // 半宽
        const uint twists = msci->twists;

        // 生成顶点
        for(uint i = 0; i <= nU; ++i)
        {
            const float u = 2.0f * math::pi * float(i) / float(nU);
            const float twist_angle = twists * u * 0.5f;  // 扭转角度
            
            const float cos_u = hgl_cos(u);
            const float sin_u = hgl_sin(u);
            const float cos_twist = hgl_cos(twist_angle);
            const float sin_twist = hgl_sin(twist_angle);
            
            // 路径中心点
            Vector3f center(R * cos_u, R * sin_u, 0);
            
            // 切向量
            Vector3f tangent(-sin_u, cos_u, 0);
            
            // 法向量（随扭转旋转）
            Vector3f normal(0, 0, 1);
            Vector3f rotated_normal;
            rotated_normal.x = 0;
            rotated_normal.y = -sin_twist;
            rotated_normal.z = cos_twist;
            
            // 副法向量（宽度方向）
            Vector3f binormal;
            binormal.x = cos_twist * cos_u;
            binormal.y = cos_twist * sin_u;
            binormal.z = sin_twist;
            
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * w * (float(j) / float(nV) - 0.5f);
                
                // 计算位置
                Vector3f pos = center + v * binormal;
                
                builder.WritePosition(pos);
                builder.WriteNormal(rotated_normal);
                
                if(builder.HasTangents())
                    builder.WriteTangent(tangent);
                
                if(builder.HasTexCoords())
                {
                    const float tex_u = float(i) / float(nU);
                    const float tex_v = float(j) / float(nV);
                    builder.WriteTexCoord(Vector2f(tex_u, tex_v));
                }
            }
        }

        // 生成索引
        if(builder.Use8BitIndex())
            IndexGenerator::CreateCylinderIndices<uint8>(builder.GetIndexBuffer<uint8>(), nU, nV);
        else if(builder.Use16BitIndex())
            IndexGenerator::CreateCylinderIndices<uint16>(builder.GetIndexBuffer<uint16>(), nU, nV);
        else
            IndexGenerator::CreateCylinderIndices<uint32>(builder.GetIndexBuffer<uint32>(), nU, nV);

        Geometry *p = builder.Finish();
        if(!p)return nullptr;

        // 计算包围盒
        const float max_extent = R + w;
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-max_extent, -max_extent, -w),
                      Vector3f(max_extent, max_extent, w));
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
