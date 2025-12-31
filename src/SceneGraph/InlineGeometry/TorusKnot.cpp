#include"InlineGeometryCommon.h"
#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    Geometry *CreateTorusKnot(GeometryCreater *pc, const TorusKnotCreateInfo *tkci)
    {
        if(!pc||!tkci)return(nullptr);

        GeometryValidator validator("TorusKnot");
        if(!validator.CheckPositive("major_radius", tkci->major_radius))return nullptr;
        if(!validator.CheckPositive("minor_radius", tkci->minor_radius))return nullptr;
        if(!validator.CheckMinValue("p", tkci->p, 1u))return nullptr;
        if(!validator.CheckMinValue("q", tkci->q, 1u))return nullptr;
        if(!validator.CheckMinValue("major_segments", tkci->major_segments, 3u))return nullptr;
        if(!validator.CheckMinValue("minor_segments", tkci->minor_segments, 3u))return nullptr;

        const uint total_vertices = (tkci->major_segments + 1) * (tkci->minor_segments + 1);
        const uint total_indices = tkci->major_segments * tkci->minor_segments * 6;

        GeometryBuilder builder(pc, total_vertices, HGL_PRIM_TRIANGLES);
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
            const float u = 2.0f * math::pi * float(i) / float(nU);
            
            // 环面纽结路径参数方程
            const float cosp_u = hgl_cos(p * u);
            const float sinp_u = hgl_sin(p * u);
            const float cosq_u = hgl_cos(q * u);
            const float sinq_u = hgl_sin(q * u);
            
            // 计算路径位置
            Vector3f path_pos;
            path_pos.x = R * (2.0f + cosp_u) * cosq_u;
            path_pos.y = R * (2.0f + cosp_u) * sinq_u;
            path_pos.z = R * sinp_u;
            
            // 计算切向量
            Vector3f tangent;
            tangent.x = -R * p * sinp_u * cosq_u - R * q * (2.0f + cosp_u) * sinq_u;
            tangent.y = -R * p * sinp_u * sinq_u + R * q * (2.0f + cosp_u) * cosq_u;
            tangent.z = R * p * cosp_u;
            tangent = normalize(tangent);
            
            // 计算径向向量（通过数值方法）
            const float delta = 0.01f;
            const float u_next = u + delta;
            Vector3f path_next;
            path_next.x = R * (2.0f + hgl_cos(p * u_next)) * hgl_cos(q * u_next);
            path_next.y = R * (2.0f + hgl_cos(p * u_next)) * hgl_sin(q * u_next);
            path_next.z = R * hgl_sin(p * u_next);
            
            Vector3f radial = path_pos - Vector3f(0, 0, 0);
            if(length(radial) < 0.001f) radial = Vector3f(0, 1, 0);
            radial = normalize(radial);
            
            // 计算副法向量
            Vector3f binormal = normalize(cross(tangent, radial));
            Vector3f normal_base = normalize(cross(binormal, tangent));
            
            // 沿管道圆周生成顶点
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * math::pi * float(j) / float(nV);
                const float cos_v = hgl_cos(v);
                const float sin_v = hgl_sin(v);
                
                // 计算截面位置
                Vector3f offset = r * (cos_v * normal_base + sin_v * binormal);
                Vector3f pos = path_pos + offset;
                
                // 法线指向截面圆心的外侧
                Vector3f normal = normalize(cos_v * normal_base + sin_v * binormal);
                
                builder.WritePosition(pos);
                builder.WriteNormal(normal);
                
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
        const float max_extent = R * 3.0f + r;
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-max_extent, -max_extent, -R - r),
                      Vector3f(max_extent, max_extent, R + r));
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
