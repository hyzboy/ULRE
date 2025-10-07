#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Geometry *CreateTaperedCapsule(GeometryCreater *pc,const TaperedCapsuleCreateInfo *tcci)
    {
        if(!pc||!tcci)return nullptr;

        const uint slices = std::max<uint>(3, tcci->numberSlices);
        const uint stacks = std::max<uint>(1, tcci->numberStacks);
        const float bottomR = tcci->bottomRadius;
        const float topR = tcci->topRadius;
        const float halfH = tcci->halfHeight;

        // We'll build: side frustum (between -halfH and +halfH) with rings at -halfH and +halfH,
        // top hemisphere centered at +halfH with radius topR, bottom hemisphere centered at -halfH with radius bottomR.

        const uint side_vertices = (slices + 1) * 2; // bottom ring and top ring
        const uint hemi_vertices = (stacks + 1) * (slices + 1);
        const uint numberVertices = side_vertices + hemi_vertices * 2;

        const uint side_faces = slices * 2;
        const uint hemi_faces = stacks * slices * 2;
        const uint numberIndices = (side_faces + hemi_faces * 2) * 3;

        if(numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;

        if(!pc->Init("TaperedCapsule", numberVertices, numberIndices))
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
        const float height = 2.0f * halfH;
        const float dr = topR - bottomR;

        // Side frustum: bottom ring at -halfH, top ring at +halfH.
        for(uint ring=0; ring<2; ++ring)
        {
            const float z = (ring==0) ? -halfH : halfH;
            const float r = (ring==0) ? bottomR : topR;

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float ca = cos(a);
                float sa = -sin(a); // keep parameterization
                float x = ca * r;
                float y = sa * r;

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    // compute outward normal for frustum side: proportional to (cos*height, -sin*height, -dr)
                    float nx = ca * height;
                    float ny = sa * height;
                    float nz = -dr;

                    float len = sqrtf(nx*nx + ny*ny + nz*nz);
                    if(len>0.0f){ nx/=len; ny/=len; nz/=len; }

                    *np = nx; ++np;
                    *np = ny; ++np;
                    *np = nz; ++np;
                }

                if(tp)
                {
                    // tangent along circumference
                    *tp = -sa; ++tp; // -(-sin)=sin? keep consistent with other files
                    *tp = -ca; ++tp;
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
            float r = cos(phi);  // ring radius, will be scaled by topR

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = r * cos(a) * topR;
                float y = -r * sin(a) * topR;
                float z = halfH + cz * topR;

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    float nx = x;
                    float ny = y;
                    float nz = cz * topR;
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
                float x = r * cos(a) * bottomR;
                float y = -r * sin(a) * bottomR;
                float z = -halfH + cz * bottomR;

                *vp = x; ++vp;
                *vp = y; ++vp;
                *vp = z; ++vp;

                if(np)
                {
                    float nx = x;
                    float ny = y;
                    float nz = cz * bottomR;
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
        const uint ringVertexCount = slices + 1;

        if(index_type==IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            uint16 *ip = ib;

            // side indices
            uint base = 0;
            for(uint i=0;i<slices;i++)
            {
                uint a = base + i;
                uint b = base + i + 1;
                uint c = base + ringVertexCount + i;
                uint d = base + ringVertexCount + i + 1;

                // maintain clockwise front
                *ip = a; ++ip; *ip = d; ++ip; *ip = b; ++ip;
                *ip = a; ++ip; *ip = c; ++ip; *ip = d; ++ip;
            }

            // top hemisphere indices
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

            // bottom hemisphere
            uint bottomBase = topBase + hemi_vertices;
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
        else if(index_type==IndexType::U32)
        {
            IBTypeMap<uint32> ib(pc->GetIBMap());
            uint32 *ip = ib;

            uint base = 0;
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

            uint bottomBase = topBase + hemi_vertices;
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

            uint bottomBase = topBase + hemi_vertices;
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

        float maxr = std::max(bottomR, topR);
        bv.SetFromAABB(Vector3f(-maxr,-maxr,-halfH-maxr), Vector3f(maxr,maxr,halfH+maxr));

        p->SetBoundingVolumes(bv);

        return p;
    }
}
