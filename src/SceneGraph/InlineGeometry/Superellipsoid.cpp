#include"InlineGeometryCommon.h"
#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    // 带符号的幂函数，保留符号
    static inline float signed_pow(float value, float exponent)
    {
        if(value >= 0.0f)
            return hgl_pow(value, exponent);
        else
            return -hgl_pow(-value, exponent);
    }

    Geometry *CreateSuperellipsoid(GeometryCreater *pc, const SuperellipsoidCreateInfo *seci)
    {
        if(!pc||!seci)return(nullptr);

        GeometryValidator validator("Superellipsoid");
        if(!validator.CheckPositive("size.x", seci->size.x))return nullptr;
        if(!validator.CheckPositive("size.y", seci->size.y))return nullptr;
        if(!validator.CheckPositive("size.z", seci->size.z))return nullptr;
        if(!validator.CheckPositive("n1", seci->n1))return nullptr;
        if(!validator.CheckPositive("n2", seci->n2))return nullptr;
        if(!validator.CheckMinValue("u_segments", seci->u_segments, 3u))return nullptr;
        if(!validator.CheckMinValue("v_segments", seci->v_segments, 2u))return nullptr;

        const uint nU = seci->u_segments;
        const uint nV = seci->v_segments;
        const uint total_vertices = (nU + 1) * (nV + 1);
        const uint total_indices = nU * nV * 6;

        GeometryBuilder builder(pc, total_vertices, HGL_PRIM_TRIANGLES);
        if(!builder.IsValid())return nullptr;

        const Vector3f size = seci->size;
        const float n1 = seci->n1;
        const float n2 = seci->n2;

        // 生成顶点 - 超椭球参数方程
        for(uint j = 0; j <= nV; ++j)
        {
            const float v = -math::pi * 0.5f + math::pi * float(j) / float(nV);  // -π/2 到 π/2
            const float cos_v = hgl_cos(v);
            const float sin_v = hgl_sin(v);
            
            // 超椭球形状函数
            const float cos_v_n2 = signed_pow(hgl_abs(cos_v), 2.0f / n2);
            const float sin_v_n2 = signed_pow(hgl_abs(sin_v), 2.0f / n2);
            const float z_factor = sin_v_n2 * (sin_v >= 0.0f ? 1.0f : -1.0f);
            
            for(uint i = 0; i <= nU; ++i)
            {
                const float u = 2.0f * math::pi * float(i) / float(nU);
                const float cos_u = hgl_cos(u);
                const float sin_u = hgl_sin(u);
                
                // 超椭球形状函数
                const float cos_u_n1 = signed_pow(hgl_abs(cos_u), 2.0f / n1);
                const float sin_u_n1 = signed_pow(hgl_abs(sin_u), 2.0f / n1);
                
                const float x_factor = cos_v_n2 * cos_u_n1 * (cos_u >= 0.0f ? 1.0f : -1.0f);
                const float y_factor = cos_v_n2 * sin_u_n1 * (sin_u >= 0.0f ? 1.0f : -1.0f);
                
                // 计算位置
                Vector3f pos;
                pos.x = size.x * x_factor;
                pos.y = size.y * y_factor;
                pos.z = size.z * z_factor;
                
                builder.WritePosition(pos);
                
                // 计算数值法线
                const float delta = 0.001f;
                
                // du方向
                const float u_next = u + delta;
                const float cos_u_next = hgl_cos(u_next);
                const float sin_u_next = hgl_sin(u_next);
                const float cos_u_next_n1 = signed_pow(hgl_abs(cos_u_next), 2.0f / n1);
                const float sin_u_next_n1 = signed_pow(hgl_abs(sin_u_next), 2.0f / n1);
                
                Vector3f pos_u_plus;
                pos_u_plus.x = size.x * cos_v_n2 * cos_u_next_n1 * (cos_u_next >= 0.0f ? 1.0f : -1.0f);
                pos_u_plus.y = size.y * cos_v_n2 * sin_u_next_n1 * (sin_u_next >= 0.0f ? 1.0f : -1.0f);
                pos_u_plus.z = pos.z;
                
                // dv方向
                const float v_next = v + delta;
                const float cos_v_next = hgl_cos(v_next);
                const float sin_v_next = hgl_sin(v_next);
                const float cos_v_next_n2 = signed_pow(hgl_abs(cos_v_next), 2.0f / n2);
                const float sin_v_next_n2 = signed_pow(hgl_abs(sin_v_next), 2.0f / n2);
                
                Vector3f pos_v_plus;
                pos_v_plus.x = size.x * cos_v_next_n2 * cos_u_n1 * (cos_u >= 0.0f ? 1.0f : -1.0f);
                pos_v_plus.y = size.y * cos_v_next_n2 * sin_u_n1 * (sin_u >= 0.0f ? 1.0f : -1.0f);
                pos_v_plus.z = size.z * sin_v_next_n2 * (sin_v_next >= 0.0f ? 1.0f : -1.0f);
                
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
        BoundingVolumes bv;
        bv.SetFromAABB(-size, size);
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
