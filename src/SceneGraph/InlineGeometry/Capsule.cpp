#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCapsule(GeometryCreater *pc,const CapsuleCreateInfo *cci)
    {
        if(!pc||!cci)return nullptr;

        // Parameters
        const uint slices = std::max<uint>(3, cci->numberSlices);
        const uint stacks = std::max<uint>(1, cci->numberStacks);
        const float radius = cci->radius;
        const float halfH = cci->halfHeight;

        // We build two hemispheres (top and bottom) and a cylinder between them.
        // Vertices: cylinder side (slices+1)*2, top hemisphere (stacks*(slices+1)), bottom hemisphere (stacks*(slices+1))
        // We'll generate as a continuous vertex stream suitable for indexed triangles.

        // Estimate counts
        const uint cylinder_side_vertices = (slices + 1) * 2; // bottom ring and top ring
        const uint hemisphere_vertices = (stacks + 1) * (slices + 1); // include pole ring
        const uint numberVertices = cylinder_side_vertices + hemisphere_vertices * 2;

        const uint cylinder_side_faces = slices * 2; // per stack (only one stack for cylinder)
        const uint cylinder_indices = cylinder_side_faces * 3;
        const uint hemisphere_faces = stacks * slices * 2; // triangles per hemisphere
        const uint numberIndices = (cylinder_indices + hemisphere_faces * 3 * 2);

        if(numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;

        if(!pc->Init("Capsule", numberVertices, numberIndices))
            return nullptr;

        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp = vertex;
        float *np = normal;
        float *tp = tangent;
        float *tcp = tex_coord;

        if(!vp) return nullptr;

        const float angleStep = (2.0f * HGL_PI) / float(slices);

        // Cylinder side: bottom ring at -halfH, top ring at +halfH
        for(uint ring=0; ring<2; ++ring)
        {
            const float z = (ring==0) ? -halfH : halfH;

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = cos(a) * radius;
                float y = -sin(a) * radius; // consistent with other shapes Y uses -sin

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    *np = x; ++np;
                    *np = y; ++np;
                    *np = 0.0f; ++np;
                }

                if(tp)
                {
                    *tp = -sin(a); ++tp;
                    *tp = -cos(a); ++tp;
                    *tp = 0.0f; ++tp;
                }

                if(tcp)
                {
                    *tcp = float(i)/float(slices); ++tcp;
                    *tcp = (ring==0)?0.0f:1.0f; ++tcp;
                }
            }
        }

        // Top hemisphere (center at +halfH)
        for(uint s=0;s<=stacks;s++)
        {
            float phi = (float)s / (float)stacks * (HGL_PI * 0.5f);
            float cz = sin(phi); // from 0 to 1
            float r = cos(phi);  // ring radius

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = r * cos(a) * radius;
                float y = -r * sin(a) * radius;
                float z = halfH + cz * radius;

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    float nx = x;
                    float ny = y;
                    float nz = cz * radius;
                    float len = sqrt(nx*nx+ny*ny+nz*nz);
                    if(len>0.0f){ nx/=len; ny/=len; nz/=len; }
                    *np = nx; ++np;
                    *np = ny; ++np;
                    *np = nz; ++np;
                }

                if(tp)
                {
                    glusQuaternionRotateRyf((float*)nullptr,0.0f); // noop to avoid unused warning in includes
                    *tp = -sin(a); ++tp;
                    *tp = -cos(a); ++tp;
                    *tp = 0.0f; ++tp;
                }

                if(tcp)
                {
                    *tcp = float(i)/float(slices); ++tcp;
                    *tcp = 1.0f - (float)s/float(stacks); ++tcp;
                }
            }
        }

        // Bottom hemisphere (center at -halfH)
        for(uint s=0;s<=stacks;s++)
        {
            float phi = (float)s / (float)stacks * (HGL_PI * 0.5f);
            float cz = -sin(phi); // from 0 to -1
            float r = cos(phi);

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = r * cos(a) * radius;
                float y = -r * sin(a) * radius;
                float z = -halfH + cz * radius;

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    float nx = x;
                    float ny = y;
                    float nz = cz * radius;
                    float len = sqrt(nx*nx+ny*ny+nz*nz);
                    if(len>0.0f){ nx/=len; ny/=len; nz/=len; }
                    *np = nx; ++np;
                    *np = ny; ++np;
                    *np = nz; ++np;
                }

                if(tp)
                {
                    *tp = -sin(a); ++tp;
                    *tp = -cos(a); ++tp;
                    *tp = 0.0f; ++tp;
                }

                if(tcp)
                {
                    *tcp = float(i)/float(slices); ++tcp;
                    *tcp = (float)s/float(stacks); ++tcp;
                }
            }
        }

        // Indices
        const IndexType index_type = pc->GetIndexType();
        if(index_type==IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            uint16 *ip = ib;

            // cylinder indices
            uint base = 0;
            uint ringVertexCount = slices + 1;
            for(uint i=0;i<slices;i++)
            {
                uint a = base + i;
                uint b = base + i + 1;
                uint c = base + ringVertexCount + i;
                uint d = base + ringVertexCount + i + 1;

                // clockwise positive face: ensure order
                *ip = a; ++ip; *ip = d; ++ip; *ip = b; ++ip;
                *ip = a; ++ip; *ip = c; ++ip; *ip = d; ++ip;
            }

            // hemispheres indices
            uint topBase = ringVertexCount*2; // after cylinder side
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = topBase + s*(slices+1);
                    uint row2 = topBase + (s+1)*(slices+1);

                    uint v0 = row1 + i;
                    uint v1 = row1 + i + 1;
                    uint v2 = row2 + i;
                    uint v3 = row2 + i + 1;

                    // top hemisphere
                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v1; ++ip;
                    *ip = v0; ++ip; *ip = v2; ++ip; *ip = v3; ++ip;
                }
            }

            // bottom hemisphere indices: follows top hemisphere vertices
            uint bottomBase = topBase + hemisphere_vertices;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = bottomBase + s*(slices+1);
                    uint row2 = bottomBase + (s+1)*(slices+1);

                    uint v0 = row1 + i;
                    uint v1 = row1 + i + 1;
                    uint v2 = row2 + i;
                    uint v3 = row2 + i + 1;

                    // bottom hemisphere: reverse winding to match outer face
                    *ip = v0; ++ip; *ip = v1; ++ip; *ip = v3; ++ip;
                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v2; ++ip;
                }
            }
        }
        else if(index_type==IndexType::U32)
        {
            IBTypeMap<uint32> ib(pc->GetIBMap());
            uint32 *ip = ib;

            uint base = 0;
            uint ringVertexCount = slices + 1;
            for(uint i=0;i<slices;i++)
            {
                uint a = base + i;
                uint b = base + i + 1;
                uint c = base + ringVertexCount + i;
                uint d = base + ringVertexCount + i + 1;

                *ip = a; ++ip; *ip = d; ++ip; *ip = b; ++ip;
                *ip = a; ++ip; *ip = c; ++ip; *ip = d; ++ip;
            }

            uint topBase = ringVertexCount*2;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = topBase + s*(slices+1);
                    uint row2 = topBase + (s+1)*(slices+1);

                    uint v0 = row1 + i;
                    uint v1 = row1 + i + 1;
                    uint v2 = row2 + i;
                    uint v3 = row2 + i + 1;

                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v1; ++ip;
                    *ip = v0; ++ip; *ip = v2; ++ip; *ip = v3; ++ip;
                }
            }

            uint bottomBase = topBase + hemisphere_vertices;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = bottomBase + s*(slices+1);
                    uint row2 = bottomBase + (s+1)*(slices+1);

                    uint v0 = row1 + i;
                    uint v1 = row1 + i + 1;
                    uint v2 = row2 + i;
                    uint v3 = row2 + i + 1;

                    *ip = v0; ++ip; *ip = v1; ++ip; *ip = v3; ++ip;
                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v2; ++ip;
                }
            }
        }
        else if(index_type==IndexType::U8)
        {
            IBTypeMap<uint8> ib(pc->GetIBMap());
            uint8 *ip = ib;

            uint base = 0;
            uint ringVertexCount = slices + 1;
            for(uint i=0;i<slices;i++)
            {
                uint8 a = base + i;
                uint8 b = base + i + 1;
                uint8 c = base + ringVertexCount + i;
                uint8 d = base + ringVertexCount + i + 1;

                *ip = a; ++ip; *ip = d; ++ip; *ip = b; ++ip;
                *ip = a; ++ip; *ip = c; ++ip; *ip = d; ++ip;
            }

            uint topBase = ringVertexCount*2;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = topBase + s*(slices+1);
                    uint row2 = topBase + (s+1)*(slices+1);

                    uint8 v0 = row1 + i;
                    uint8 v1 = row1 + i + 1;
                    uint8 v2 = row2 + i;
                    uint8 v3 = row2 + i + 1;

                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v1; ++ip;
                    *ip = v0; ++ip; *ip = v2; ++ip; *ip = v3; ++ip;
                }
            }

            uint bottomBase = topBase + hemisphere_vertices;
            for(uint s=0;s<stacks;s++)
            {
                for(uint i=0;i<slices;i++)
                {
                    uint row1 = bottomBase + s*(slices+1);
                    uint row2 = bottomBase + (s+1)*(slices+1);

                    uint8 v0 = row1 + i;
                    uint8 v1 = row1 + i + 1;
                    uint8 v2 = row2 + i;
                    uint8 v3 = row2 + i + 1;

                    *ip = v0; ++ip; *ip = v1; ++ip; *ip = v3; ++ip;
                    *ip = v0; ++ip; *ip = v3; ++ip; *ip = v2; ++ip;
                }
            }
        }

        Geometry *p = pc->Create();

        BoundingVolumes bv;

        bv.SetFromAABB(math::Vector3f(-radius,-radius,-halfH-radius),
                       Vector3f(radius,radius,halfH+radius));

        p->SetBoundingVolumes(bv);

        return p;
    }
}
