#include"InlineGeometryCommon.h"
#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    Geometry *CreateKleinBottle(GeometryCreater *pc, const KleinBottleCreateInfo *kbci)
    {
        if(!pc||!kbci)return(nullptr);

        GeometryValidator validator("KleinBottle");
        if(!validator.CheckPositive("major_radius", kbci->major_radius))return nullptr;
        if(!validator.CheckPositive("minor_radius", kbci->minor_radius))return nullptr;
        if(!validator.CheckMinValue("u_segments", kbci->u_segments, 3u))return nullptr;
        if(!validator.CheckMinValue("v_segments", kbci->v_segments, 3u))return nullptr;

        const uint nU = kbci->u_segments;
        const uint nV = kbci->v_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        GeometryBuilder builder(pc, total_vertices, HGL_PRIM_TRIANGLES);
        if(!builder.IsValid())return nullptr;

        const float R = kbci->major_radius;
        const float r = kbci->minor_radius;

        // 生成顶点 - 使用克莱因瓶的参数方程（figure-8沉浸）
        for(uint i = 0; i <= nU; ++i)
        {
            const float u = 2.0f * math::pi * float(i) / float(nU);
            const float cos_u = hgl_cos(u);
            const float sin_u = hgl_sin(u);
            
            for(uint j = 0; j <= nV; ++j)
            {
                const float v = 2.0f * math::pi * float(j) / float(nV);
                const float cos_v = hgl_cos(v);
                const float sin_v = hgl_sin(v);
                
                // 克莱因瓶参数方程（改进的figure-8形式）
                Vector3f pos;
                
                if(u < math::pi)
                {
                    // 外部部分
                    pos.x = (R + r * cos_v) * cos_u;
                    pos.y = (R + r * cos_v) * sin_u;
                    pos.z = r * sin_v;
                }
                else
                {
                    // 穿过自身的部分
                    pos.x = (R + r * cos_v) * cos_u;
                    pos.y = (R + r * cos_v) * sin_u;
                    pos.z = r * sin_v;
                }
                
                // 添加figure-8扭转
                const float twist = hgl_sin(u);
                pos.x += twist * r * 0.5f * sin_v * cos_u;
                pos.y += twist * r * 0.5f * sin_v * sin_u;
                
                builder.WritePosition(pos);
                
                // 计算数值法线
                const float delta = 0.01f;
                
                Vector3f pos_u_plus, pos_v_plus;
                float u_next = u + delta;
                float v_next = v + delta;
                
                // du方向
                float cos_u_next = hgl_cos(u_next);
                float sin_u_next = hgl_sin(u_next);
                float twist_next = hgl_sin(u_next);
                
                if(u_next < math::pi)
                {
                    pos_u_plus.x = (R + r * cos_v) * cos_u_next + twist_next * r * 0.5f * sin_v * cos_u_next;
                    pos_u_plus.y = (R + r * cos_v) * sin_u_next + twist_next * r * 0.5f * sin_v * sin_u_next;
                    pos_u_plus.z = r * sin_v;
                }
                else
                {
                    pos_u_plus.x = (R + r * cos_v) * cos_u_next + twist_next * r * 0.5f * sin_v * cos_u_next;
                    pos_u_plus.y = (R + r * cos_v) * sin_u_next + twist_next * r * 0.5f * sin_v * sin_u_next;
                    pos_u_plus.z = r * sin_v;
                }
                
                // dv方向
                float cos_v_next = hgl_cos(v_next);
                float sin_v_next = hgl_sin(v_next);
                
                if(u < math::pi)
                {
                    pos_v_plus.x = (R + r * cos_v_next) * cos_u + twist * r * 0.5f * sin_v_next * cos_u;
                    pos_v_plus.y = (R + r * cos_v_next) * sin_u + twist * r * 0.5f * sin_v_next * sin_u;
                    pos_v_plus.z = r * sin_v_next;
                }
                else
                {
                    pos_v_plus.x = (R + r * cos_v_next) * cos_u + twist * r * 0.5f * sin_v_next * cos_u;
                    pos_v_plus.y = (R + r * cos_v_next) * sin_u + twist * r * 0.5f * sin_v_next * sin_u;
                    pos_v_plus.z = r * sin_v_next;
                }
                
                Vector3f du = pos_u_plus - pos;
                Vector3f dv = pos_v_plus - pos;
                Vector3f normal = normalize(cross(du, dv));
                
                builder.WriteNormal(normal);
                
                if(builder.HasTangents())
                    builder.WriteTangent(normalize(du));
                
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
        const float max_extent = R + r * 1.5f;
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-max_extent, -max_extent, -r),
                      Vector3f(max_extent, max_extent, r));
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
