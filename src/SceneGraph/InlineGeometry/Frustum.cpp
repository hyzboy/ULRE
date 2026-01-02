// Frustum (truncated cone) geometry generator for ULRE engine
// Similar to cylinder but with different top and bottom radii

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateFrustum(GeometryCreater *pc, const FrustumCreateInfo *fci)
    {
        if(!pc || !fci)
            return nullptr;

        const uint slices = fci->numberSlices;
        
        // Validate parameters
        if(!GeometryValidator::ValidateSlices(slices))
            return nullptr;

        // Calculate vertex and index counts
        uint numberVertices = 0;
        uint numberIndices = 0;

        if(fci->generate_caps)
        {
            // Bottom cap: center + ring
            numberVertices += 1 + (slices + 1);
            numberIndices += slices * 3;
            
            // Top cap: center + ring
            numberVertices += 1 + (slices + 1);
            numberIndices += slices * 3;
        }

        // Side surface: two rings
        numberVertices += (slices + 1) * 2;
        numberIndices += slices * 6;

        // Validate counts
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Frustum", numberVertices, numberIndices))
            return nullptr;

        const float angleStep = (2.0f * std::numbers::pi_v<float>) / float(slices);
        const float halfHeight = fci->height * 0.5f;
        const float r_bottom = fci->bottom_radius;
        const float r_top = fci->top_radius;

        // Calculate normal direction for slanted side
        // Normal is perpendicular to the slant surface
        const float dr = r_bottom - r_top;
        const float h = fci->height;
        const float slant_length = sqrtf(dr * dr + h * h);
        const float normal_z = dr / slant_length;  // Z component of side normal
        const float normal_xy_scale = h / slant_length;  // XY scale for side normal

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        uint base_index = 0;

        // Generate caps if requested
        if(fci->generate_caps)
        {
            // Bottom center vertex
            builder.WriteFullVertex(0.0f, 0.0f, -halfHeight,
                                   0.0f, 0.0f, -1.0f,
                                   0.0f, 1.0f, 0.0f,
                                   0.5f, 0.5f);

            // Bottom ring vertices
            for(uint i = 0; i <= slices; i++)
            {
                float currentAngle = angleStep * float(i);
                float cosAngle = cos(currentAngle);
                float sinAngle = sin(currentAngle);

                builder.WriteFullVertex(cosAngle * r_bottom, -sinAngle * r_bottom, -halfHeight,
                                       0.0f, 0.0f, -1.0f,
                                       sinAngle, cosAngle, 0.0f,
                                       cosAngle * 0.5f + 0.5f, -sinAngle * 0.5f + 0.5f);
            }

            // Top center vertex
            builder.WriteFullVertex(0.0f, 0.0f, halfHeight,
                                   0.0f, 0.0f, 1.0f,
                                   0.0f, -1.0f, 0.0f,
                                   0.5f, 0.5f);

            // Top ring vertices
            for(uint i = 0; i <= slices; i++)
            {
                float currentAngle = angleStep * float(i);
                float cosAngle = cos(currentAngle);
                float sinAngle = sin(currentAngle);

                builder.WriteFullVertex(cosAngle * r_top, -sinAngle * r_top, halfHeight,
                                       0.0f, 0.0f, 1.0f,
                                       -sinAngle, -cosAngle, 0.0f,
                                       cosAngle * 0.5f + 0.5f, -sinAngle * 0.5f + 0.5f);
            }

            base_index = 2 + (slices + 1) * 2;
        }

        // Side vertices (two rings for proper normals)
        for(uint i = 0; i <= slices; i++)
        {
            float currentAngle = angleStep * float(i);
            float cosAngle = cos(currentAngle);
            float sinAngle = sin(currentAngle);

            // Normal for side surface
            float nx = cosAngle * normal_xy_scale;
            float ny = -sinAngle * normal_xy_scale;
            float nz = normal_z;

            // Tangent is perpendicular to normal in XY plane
            float tx = -sinAngle;
            float ty = -cosAngle;

            // Bottom vertex
            builder.WriteFullVertex(cosAngle * r_bottom, -sinAngle * r_bottom, -halfHeight,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 0.0f);

            // Top vertex
            builder.WriteFullVertex(cosAngle * r_top, -sinAngle * r_top, halfHeight,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            if(fci->generate_caps)
            {
                // Bottom cap indices
                IndexT centerIndex = 0;
                IndexT ringStart = 1;
                
                for(uint i = 0; i < slices; i++)
                {
                    *ip++ = centerIndex;
                    *ip++ = ringStart + i;
                    *ip++ = ringStart + i + 1;
                }

                // Top cap indices
                centerIndex = 1 + (slices + 1);
                ringStart = centerIndex + 1;

                for(uint i = 0; i < slices; i++)
                {
                    *ip++ = centerIndex;
                    *ip++ = ringStart + i + 1;
                    *ip++ = ringStart + i;
                }
            }

            // Side indices
            IndexT sideBase = base_index;
            
            for(uint i = 0; i < slices; i++)
            {
                IndexT v0 = sideBase + i * 2;
                IndexT v1 = sideBase + i * 2 + 1;
                IndexT v2 = sideBase + (i + 1) * 2;
                IndexT v3 = sideBase + (i + 1) * 2 + 1;

                *ip++ = v0;
                *ip++ = v1;
                *ip++ = v2;

                *ip++ = v2;
                *ip++ = v1;
                *ip++ = v3;
            }
        };

        if(index_type == IndexType::U16)
        {
            IBTypeMap<uint16> ib(pc->GetIBMap());
            uint16 *ip = ib;
            generate_indices(ip);
        }
        else if(index_type == IndexType::U32)
        {
            IBTypeMap<uint32> ib(pc->GetIBMap());
            uint32 *ip = ib;
            generate_indices(ip);
        }
        else if(index_type == IndexType::U8)
        {
            IBTypeMap<uint8> ib(pc->GetIBMap());
            uint8 *ip = ib;
            generate_indices(ip);
        }
        else
            return nullptr;

        Geometry *p = pc->Create();

        // Set bounding box
        BoundingVolumes bv;
        float max_radius = std::max(r_bottom, r_top);
        bv.SetFromAABB(math::Vector3f(-max_radius, -max_radius, -halfHeight),
                       Vector3f(max_radius, max_radius, halfHeight));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
