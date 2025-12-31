// Crescent geometry generator for ULRE engine
// Creates a 3D crescent/moon shape

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCrescent(GeometryCreater *pc, const CrescentCreateInfo *cci)
    {
        if(!pc || !cci)
            return nullptr;

        const float outer_r = cci->outer_radius;
        const float inner_r = cci->inner_radius;
        const float thickness = cci->thickness;
        const float offset = cci->offset;
        const uint segments = std::max<uint>(8, cci->segments);

        const float half_thickness = thickness * 0.5f;

        // Validate parameters
        if(outer_r <= 0.0f || inner_r <= 0.0f || thickness <= 0.0f)
            return nullptr;

        if(inner_r >= outer_r)
            return nullptr;

        // Crescent is the difference between two circles
        // We need to find the intersection points to determine the arc range
        
        // Calculate intersection angles
        // Using circle-circle intersection mathematics
        float d = offset;  // Distance between centers
        
        if(d >= outer_r + inner_r || d + inner_r <= outer_r)
        {
            // Circles don't intersect properly for crescent
            return nullptr;
        }

        // Find intersection points using law of cosines
        float angle_outer = acos((outer_r * outer_r + d * d - inner_r * inner_r) / (2.0f * outer_r * d));
        
        // Crescent outer arc goes from -angle_outer to +angle_outer
        // Inner arc center is offset by (offset, 0)
        
        const uint outer_verts = segments + 1;
        const uint inner_verts = segments + 1;
        const uint profile_verts = outer_verts + inner_verts;

        // Vertices:
        // - Front face: outer arc + inner arc
        // - Back face: outer arc + inner arc
        // - Outer side: outer_verts * 2
        // - Inner side: inner_verts * 2
        const uint front_verts = profile_verts;
        const uint back_verts = profile_verts;
        const uint side_verts = profile_verts * 2;
        const uint numberVertices = front_verts + back_verts + side_verts;

        // Indices:
        // - Front cap: (outer_verts + inner_verts - 2) * 3 (approximate)
        // - Back cap: similar
        // - Sides: profile_verts * 6
        const uint numberIndices = (profile_verts - 1) * 3 * 2 + profile_verts * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Crescent", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Front face - outer arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            float angle = -angle_outer + 2.0f * angle_outer * t;
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            builder.WriteFullVertex(x, y, half_thickness,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   (x / outer_r + 1.0f) * 0.5f, (y / outer_r + 1.0f) * 0.5f);
        }

        // Front face - inner arc (reverse direction for proper winding)
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(segments - i) / float(segments);
            
            // Calculate angle for inner circle centered at (offset, 0)
            float inner_angle_range = math::pi - angle_outer;
            float angle = -inner_angle_range + 2.0f * inner_angle_range * t;
            
            float x = offset + cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            builder.WriteFullVertex(x, y, half_thickness,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   (x / outer_r + 1.0f) * 0.5f, (y / outer_r + 1.0f) * 0.5f);
        }

        // Back face - outer arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            float angle = -angle_outer + 2.0f * angle_outer * t;
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   (x / outer_r + 1.0f) * 0.5f, (y / outer_r + 1.0f) * 0.5f);
        }

        // Back face - inner arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(segments - i) / float(segments);
            float inner_angle_range = math::pi - angle_outer;
            float angle = -inner_angle_range + 2.0f * inner_angle_range * t;
            
            float x = offset + cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   (x / outer_r + 1.0f) * 0.5f, (y / outer_r + 1.0f) * 0.5f);
        }

        // Side vertices - outer arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            float angle = -angle_outer + 2.0f * angle_outer * t;
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            float nx = cos(angle);
            float ny = sin(angle);
            float nz = 0.0f;

            float tx = -sin(angle);
            float ty = cos(angle);
            float tz = 0.0f;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   nx, ny, nz, tx, ty, tz, t, 0.0f);

            builder.WriteFullVertex(x, y, half_thickness,
                                   nx, ny, nz, tx, ty, tz, t, 1.0f);
        }

        // Side vertices - inner arc (inverted normals)
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            float inner_angle_range = math::pi - angle_outer;
            float angle = -inner_angle_range + 2.0f * inner_angle_range * t;
            
            float x = offset + cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            float nx = -cos(angle);
            float ny = -sin(angle);
            float nz = 0.0f;

            float tx = -sin(angle);
            float ty = cos(angle);
            float tz = 0.0f;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   nx, ny, nz, tx, ty, tz, t, 0.0f);

            builder.WriteFullVertex(x, y, half_thickness,
                                   nx, ny, nz, tx, ty, tz, t, 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Front cap - triangulate between outer and inner arcs
            for(uint i = 0; i < profile_verts - 1; i++)
            {
                *ip++ = 0;  // Start vertex
                *ip++ = i + 1;
                *ip++ = i;
            }

            // Back cap
            IndexT back_base = front_verts;
            for(uint i = 0; i < profile_verts - 1; i++)
            {
                *ip++ = back_base;
                *ip++ = back_base + i;
                *ip++ = back_base + i + 1;
            }

            // Outer side
            IndexT outer_side_base = front_verts + back_verts;
            for(uint i = 0; i < segments; i++)
            {
                IndexT v0 = outer_side_base + i * 2;
                IndexT v1 = outer_side_base + i * 2 + 1;
                IndexT v2 = outer_side_base + (i + 1) * 2;
                IndexT v3 = outer_side_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
            }

            // Inner side
            IndexT inner_side_base = outer_side_base + (segments + 1) * 2;
            for(uint i = 0; i < segments; i++)
            {
                IndexT v0 = inner_side_base + i * 2;
                IndexT v1 = inner_side_base + i * 2 + 1;
                IndexT v2 = inner_side_base + (i + 1) * 2;
                IndexT v3 = inner_side_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v2; *ip++ = v1;
                *ip++ = v2; *ip++ = v3; *ip++ = v1;
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
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, -half_thickness),
                       Vector3f(outer_r + offset, outer_r, half_thickness));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
