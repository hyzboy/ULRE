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

        // Estimate counts
        const uint cylinder_side_vertices = (slices + 1) * 2;
        const uint hemisphere_vertices = (stacks + 1) * (slices + 1);
        const uint numberVertices = cylinder_side_vertices + hemisphere_vertices * 2;

        const uint cylinder_side_faces = slices * 2;
        const uint cylinder_indices = cylinder_side_faces * 3;
        const uint hemisphere_faces = stacks * slices * 2;
        const uint numberIndices = (cylinder_indices + hemisphere_faces * 3 * 2);

        // Validate parameters using new validator
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Capsule", numberVertices, numberIndices))
            return nullptr;

        // Use new GeometryBuilder for vertex attribute management
        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angleStep = (2.0f * std::numbers::pi_v<float>) / float(slices);

        // Cylinder side: bottom ring at -halfH, top ring at +halfH
        for(uint ring=0; ring<2; ++ring)
        {
            const float z = (ring==0) ? -halfH : halfH;

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = cos(a) * radius;
                float y = -sin(a) * radius;

                builder.WriteFullVertex(x, y, z,
                                       x, y, 0.0f,
                                       -sin(a), -cos(a), 0.0f,
                                       float(i)/float(slices), (ring==0)?0.0f:1.0f);
            }
        }

        // Top hemisphere (center at +halfH)
        for(uint s=0;s<=stacks;s++)
        {
            float phi = (float)s / (float)stacks * (std::numbers::pi_v<float> * 0.5f);
            float cz = sin(phi);
            float r = cos(phi);

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = r * cos(a) * radius;
                float y = -r * sin(a) * radius;
                float z = halfH + cz * radius;

                float nx = x;
                float ny = y;
                float nz = cz * radius;
                float len = sqrt(nx*nx+ny*ny+nz*nz);
                if(len>0.0f){ nx/=len; ny/=len; nz/=len; }
                
                builder.WriteFullVertex(x, y, z,
                                       nx, ny, nz,
                                       -sin(a), -cos(a), 0.0f,
                                       float(i)/float(slices), 1.0f - (float)s/float(stacks));
            }
        }

        // Bottom hemisphere (center at -halfH)
        for(uint s=0;s<=stacks;s++)
        {
            float phi = (float)s / (float)stacks * (std::numbers::pi_v<float> * 0.5f);
            float cz = -sin(phi);
            float r = cos(phi);

            for(uint i=0;i<=slices;i++)
            {
                float a = angleStep * float(i);
                float x = r * cos(a) * radius;
                float y = -r * sin(a) * radius;
                float z = -halfH + cz * radius;

                float nx = x;
                float ny = y;
                float nz = cz * radius;
                float len = sqrt(nx*nx+ny*ny+nz*nz);
                if(len>0.0f){ nx/=len; ny/=len; nz/=len; }

                builder.WriteFullVertex(x, y, z,
                                       nx, ny, nz,
                                       -sin(a), -cos(a), 0.0f,
                                       float(i)/float(slices), (float)s/float(stacks));
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
