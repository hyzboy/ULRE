// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include <algorithm>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    Geometry *CreateHollowCylinder(GeometryCreater *pc,const HollowCylinderCreateInfo *hcci)
    {
        if(!pc||!hcci) return nullptr;

        if(hcci->innerRadius<=0)        //如果内半径为0，则退化为实心圆柱
        {
            CylinderCreateInfo cci;

            cci.halfExtend=hcci->halfExtend;
            cci.radius=hcci->outerRadius;
            cci.numberSlices=hcci->numberSlices;

            return CreateCylinder(pc,&cci);
        }

        const uint slices = (hcci->numberSlices<3)?3:hcci->numberSlices;
        float r_in = hcci->innerRadius;
        float r_out= hcci->outerRadius;
        if(r_in>r_out){ float tmp=r_in; r_in=r_out; r_out=tmp; }
        const float r0 = r_in;
        const float r1 = r_out;
        const float he = hcci->halfExtend;

        // Vertices count: walls (outer + inner) + caps (top + bottom)
        const uint wall_vert_per_side = (slices + 1) * 2; // for strip: (slices+1) columns, 2 rows (bottom/top)
        const uint cap_vert_per = (slices + 1) * 2;       // outer+inner per slice step

        const uint numberVertices = wall_vert_per_side * 2 + cap_vert_per * 2;
        const uint numberIndices = (slices * 2 /*two walls*/ + slices * 2 /*two caps*/) * 6; // each slice -> 2 tris -> 6 indices per triangle

        if(!pc->Init("HollowCylinder", numberVertices, numberIndices))
            return nullptr;

        VABMapFloat pos   (pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm   (pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tan   (pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat uv    (pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos;
        float *np = nrm;
        float *tp = tan;
        float *uvp= uv;

        const float dtheta = (2.0f * HGL_PI) / float(slices);

        auto write_vertex = [&](float x,float y,float z, float nx, float ny, float nz, float u, float v)
            {
                if(vp){ *vp++=x; *vp++=y; *vp++=z; }
                if(np){ *np++=nx; *np++=ny; *np++=nz; }
                if(tp){
                    // 壁面：切线 = 法线绕Z旋90度；端盖：给稳定切线(1,0,0)
                    if(fabsf(nz) > 0.5f){ *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
                    else                 { *tp++ = -ny;  *tp++ =  nx;  *tp++ = 0.0f; }
                }
                if(uvp){ *uvp++=u; *uvp++=v; }
            };

        // Track base indices
        const uint wall_outer_start = 0;
        auto write_wall = [&](float radius, bool outer)
            {
                for(uint i=0;i<=slices;i++)
                {
                    float ang = dtheta * float(i);
                    float cx = cos(ang), sy = -sin(ang); // 保持与 CreateCylinder 相同的参数化
                    // wall normal: outward from surface
                    float nx = outer? cx : -cx;
                    float ny = outer? sy : -sy;
                    float u = float(i) / float(slices);
                    // bottom
                    write_vertex(radius*cx, radius*sy, -he, nx, ny, 0.0f, u, 0.0f);
                    // top
                    write_vertex(radius*cx, radius*sy,  he, nx, ny, 0.0f, u, 1.0f);
                }
            };

        write_wall(r1, true);
        const uint wall_inner_start = wall_outer_start + wall_vert_per_side;
        write_wall(r0, false);

        auto write_cap = [&](float z, bool top)
            {
                for(uint i=0;i<=slices;i++)
                {
                    float ang = dtheta * float(i);
                    float cx = cos(ang), sy = -sin(ang);
                    float nx = 0.0f, ny = 0.0f, nz = top? 1.0f : -1.0f;
                    float u = (float(i) / float(slices)) * hcci->cap_angular_tiles;
                    float v_outer = hcci->cap_radial_tiles; // at r1
                    float v_inner = 0.0f;                   // at r0
                    write_vertex(r1*cx, r1*sy, z, nx, ny, nz, u, v_outer);
                    write_vertex(r0*cx, r0*sy, z, nx, ny, nz, u, v_inner);
                }
            };

        const uint cap_top_start = wall_inner_start + wall_vert_per_side;
        write_cap(+he, true);
        const uint cap_bottom_start = cap_top_start + cap_vert_per;
        write_cap(-he, false);

        // Indices
        IBMap *ib_map = pc->GetIBMap();
        const IndexType it = pc->GetIndexType();

        auto emit_wall_indices = [&](auto *ip, uint base, bool invert)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint v0 = base + i*2;
                    uint v1 = base + i*2 + 1;
                    uint v2 = base + (i+1)*2;
                    uint v3 = base + (i+1)*2 + 1;
                    if(!invert)
                    {   // 外壁：与 Cylinder 一致（正面朝外）
                        *ip++ = (decltype(*ip))v0; *ip++ = (decltype(*ip))v1; *ip++ = (decltype(*ip))v2;
                        *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v1; *ip++ = (decltype(*ip))v3;
                    }
                    else
                    {   // 内壁：反向（正面朝内）
                        *ip++ = (decltype(*ip))v0; *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v1;
                        *ip++ = (decltype(*ip))v2; *ip++ = (decltype(*ip))v3; *ip++ = (decltype(*ip))v1;
                    }
                }
                return ip;
            };

        auto emit_cap_indices = [&](auto *ip, uint base, bool top)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint o0 = base + i*2;
                    uint i0 = base + i*2 + 1;
                    uint o1 = base + (i+1)*2;
                    uint i1 = base + (i+1)*2 + 1;
                    if(top)
                    {   // 顶盖：法线+Z，俯视为正面
                        *ip++ = (decltype(*ip))o0; *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))o1;
                        *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))i1; *ip++ = (decltype(*ip))o1;
                    }
                    else
                    {   // 底盖：法线-Z，从 -Z 看为正面
                        *ip++ = (decltype(*ip))o0; *ip++ = (decltype(*ip))o1; *ip++ = (decltype(*ip))i0;
                        *ip++ = (decltype(*ip))i0; *ip++ = (decltype(*ip))i1; *ip++ = (decltype(*ip))o1;
                    }
                }
                return ip;
            };

        if(it==IndexType::U16)
        {
            IBTypeMap<uint16> im(ib_map);
            uint16 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else if(it==IndexType::U32)
        {
            IBTypeMap<uint32> im(ib_map);
            uint32 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else if(it==IndexType::U8)
        {
            IBTypeMap<uint8> im(ib_map);
            uint8 *ip = im;
            ip = emit_wall_indices(ip, wall_outer_start, false);
            ip = emit_wall_indices(ip, wall_inner_start, true);
            ip = emit_cap_indices(ip, cap_top_start, true);
            ip = emit_cap_indices(ip, cap_bottom_start, false);
        }
        else return nullptr;

        Geometry *p = pc->Create();
        if(p)
        {
            math::BoundingVolumes bv;
            bv.SetFromAABB(math::Vector3f(-r1,-r1,-he), math::Vector3f(r1,r1,he));
            p->SetBoundingVolumes(bv);
        }
        return p;
    }
}//namespace hgl::graph::inline_geometry
