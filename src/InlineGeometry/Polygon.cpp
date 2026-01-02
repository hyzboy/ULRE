// Regular polygon geometry generator for ULRE engine
// Creates a 3D regular polygon (triangle, pentagon, hexagon, etc.)

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreatePolygon(GeometryCreater *pc, const PolygonCreateInfo *pci)
    {
        if(!pc || !pci)
            return nullptr;

        const uint sides = std::max<uint>(3, pci->sides);
        const float radius = pci->radius;
        const float depth = pci->depth;
        const float half_depth = depth * 0.5f;
        const bool centered = pci->centered;

        // Validate parameters
        if(radius <= 0.0f || depth <= 0.0f)
            return nullptr;

        // Vertices:
        // - Front face: center + sides vertices
        // - Back face: center + sides vertices
        // - Side: sides * 2 vertices
        const uint profile_verts = sides + 1;
        const uint front_verts = 1 + profile_verts;
        const uint back_verts = 1 + profile_verts;
        const uint side_verts = profile_verts * 2;
        const uint numberVertices = front_verts + back_verts + side_verts;

        // Indices:
        // - Front cap: sides triangles
        // - Back cap: sides triangles
        // - Side: sides quads
        const uint numberIndices = sides * 3 + sides * 3 + sides * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Polygon", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Angle offset: if centered, start at top; otherwise start at side
        float angle_offset = centered ? (std::numbers::pi_v<float> * 0.5f) : 0.0f;
        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(sides);

        // Front face center
        builder.WriteFullVertex(0.0f, 0.0f, half_depth,
                               0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Front face vertices
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_offset + angle_step * float(i);
            float x = cos(angle) * radius;
            float y = sin(angle) * radius;

            builder.WriteFullVertex(x, y, half_depth,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   x / (radius * 2.0f) + 0.5f, y / (radius * 2.0f) + 0.5f);
        }

        // Back face center
        builder.WriteFullVertex(0.0f, 0.0f, -half_depth,
                               0.0f, 0.0f, -1.0f,
                               -1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Back face vertices
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_offset + angle_step * float(i);
            float x = cos(angle) * radius;
            float y = sin(angle) * radius;

            builder.WriteFullVertex(x, y, -half_depth,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   x / (radius * 2.0f) + 0.5f, y / (radius * 2.0f) + 0.5f);
        }

        // Side vertices
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_offset + angle_step * float(i);
            float x = cos(angle) * radius;
            float y = sin(angle) * radius;

            // Normal pointing outward
            float nx = cos(angle);
            float ny = sin(angle);
            float nz = 0.0f;

            float tx = -sin(angle);
            float ty = cos(angle);
            float tz = 0.0f;

            // Bottom vertex
            builder.WriteFullVertex(x, y, -half_depth,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(sides), 0.0f);

            // Top vertex
            builder.WriteFullVertex(x, y, half_depth,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(sides), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Front cap
            IndexT front_center = 0;
            IndexT front_ring = 1;

            for(uint i = 0; i < sides; i++)
            {
                *ip++ = front_center;
                *ip++ = front_ring + i + 1;
                *ip++ = front_ring + i;
            }

            // Back cap
            IndexT back_center = front_verts;
            IndexT back_ring = back_center + 1;

            for(uint i = 0; i < sides; i++)
            {
                *ip++ = back_center;
                *ip++ = back_ring + i;
                *ip++ = back_ring + i + 1;
            }

            // Side faces
            IndexT side_base = front_verts + back_verts;

            for(uint i = 0; i < sides; i++)
            {
                IndexT v0 = side_base + i * 2;
                IndexT v1 = side_base + i * 2 + 1;
                IndexT v2 = side_base + (i + 1) * 2;
                IndexT v3 = side_base + (i + 1) * 2 + 1;

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

        return pc->CreateWithAABB(
            math::Vector3f(-radius, -radius, -half_depth),
            Vector3f(radius, radius, half_depth));
    }
} // namespace hgl::graph::inline_geometry
