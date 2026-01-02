// Star geometry generator for ULRE engine
// Creates a 3D star shape with configurable points and radii

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateStar(GeometryCreater *pc, const StarCreateInfo *sci)
    {
        if(!pc || !sci)
            return nullptr;

        const uint points = std::max<uint>(3, sci->points);
        const uint segs_per_point = std::max<uint>(1, sci->segments_per_point);
        const float outer_r = sci->outer_radius;
        const float inner_r = sci->inner_radius;
        const float depth = sci->depth;
        const float half_depth = depth * 0.5f;

        // Validate parameters
        if(outer_r <= 0.0f || inner_r <= 0.0f || depth <= 0.0f)
            return nullptr;

        if(inner_r >= outer_r)
            return nullptr;

        // Total vertices around star: points * 2 (alternating outer/inner) * segs_per_point
        const uint profile_segments = points * 2;
        const uint profile_verts = profile_segments + 1;

        // Vertices:
        // - Front face: center + profile ring
        // - Back face: center + profile ring
        // - Side: profile ring * 2 (top and bottom)
        const uint front_verts = 1 + profile_verts;
        const uint back_verts = 1 + profile_verts;
        const uint side_verts = profile_verts * 2;
        const uint numberVertices = front_verts + back_verts + side_verts;

        // Indices:
        // - Front cap: profile_segments triangles
        // - Back cap: profile_segments triangles
        // - Side: profile_segments quads
        const uint numberIndices = profile_segments * 3 + profile_segments * 3 + profile_segments * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Star", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(profile_segments);

        // Front face center
        builder.WriteFullVertex(0.0f, 0.0f, half_depth,
                               0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Front face profile
        for(uint i = 0; i <= profile_segments; i++)
        {
            float angle = angle_step * float(i);
            // Alternate between outer and inner radius
            bool is_outer = (i % 2) == 0;
            float r = is_outer ? outer_r : inner_r;
            
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            builder.WriteFullVertex(x, y, half_depth,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   x / (outer_r * 2.0f) + 0.5f, -y / (outer_r * 2.0f) + 0.5f);
        }

        // Back face center
        builder.WriteFullVertex(0.0f, 0.0f, -half_depth,
                               0.0f, 0.0f, -1.0f,
                               -1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Back face profile
        for(uint i = 0; i <= profile_segments; i++)
        {
            float angle = angle_step * float(i);
            bool is_outer = (i % 2) == 0;
            float r = is_outer ? outer_r : inner_r;
            
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            builder.WriteFullVertex(x, y, -half_depth,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   x / (outer_r * 2.0f) + 0.5f, -y / (outer_r * 2.0f) + 0.5f);
        }

        // Side vertices
        for(uint i = 0; i <= profile_segments; i++)
        {
            float angle = angle_step * float(i);
            bool is_outer = (i % 2) == 0;
            float r = is_outer ? outer_r : inner_r;
            
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            // Normal pointing outward
            float nx = cos(angle);
            float ny = -sin(angle);
            float nz = 0.0f;

            float tx = -sin(angle);
            float ty = -cos(angle);
            float tz = 0.0f;

            // Bottom vertex
            builder.WriteFullVertex(x, y, -half_depth,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(profile_segments), 0.0f);

            // Top vertex
            builder.WriteFullVertex(x, y, half_depth,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(profile_segments), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Front cap
            IndexT front_center = 0;
            IndexT front_ring = 1;

            for(uint i = 0; i < profile_segments; i++)
            {
                *ip++ = front_center;
                *ip++ = front_ring + i + 1;
                *ip++ = front_ring + i;
            }

            // Back cap
            IndexT back_center = front_verts;
            IndexT back_ring = back_center + 1;

            for(uint i = 0; i < profile_segments; i++)
            {
                *ip++ = back_center;
                *ip++ = back_ring + i;
                *ip++ = back_ring + i + 1;
            }

            // Side faces
            IndexT side_base = front_verts + back_verts;

            for(uint i = 0; i < profile_segments; i++)
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

        Geometry *p = pc->Create();

        // Set bounding box
        BoundingVolumes bv;
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, -half_depth),
                       Vector3f(outer_r, outer_r, half_depth));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
