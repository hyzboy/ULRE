// Solid gear geometry generator for ULRE engine
// Creates a solid cylindrical gear with teeth around the perimeter

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateSolidGear(GeometryCreater *pc, const SolidGearCreateInfo *sgci)
    {
        if(!pc || !sgci)
            return nullptr;

        const uint tooth_count = std::max<uint>(3, sgci->tooth_count);
        const uint segs_per_tooth = std::max<uint>(1, sgci->segments_per_tooth);
        const float outer_r = sgci->outer_radius;
        const float root_r = sgci->root_radius;
        const float thickness = sgci->thickness;
        const float tooth_width_ratio = std::clamp(sgci->tooth_width_ratio, 0.1f, 0.9f);

        // Validate parameters
        if(root_r >= outer_r)
            return nullptr;

        if(thickness <= 0.0f)
            return nullptr;

        // Each tooth consists of:
        // - tooth top (at outer_radius)
        // - tooth sides (transitions)
        // - tooth root (at root_radius)
        // Total segments around the gear
        const uint total_segments = tooth_count * segs_per_tooth * 4;  // 4 segments per tooth (up, top, down, root)

        // Vertices:
        // - Top face: center + ring of vertices
        // - Bottom face: center + ring of vertices
        // - Side: ring of vertices (top and bottom)
        const uint ring_verts = total_segments + 1;
        const uint numberVertices = 2 + ring_verts * 2 + ring_verts * 2;  // 2 centers + 2 cap rings + 2 side rings

        // Indices:
        // - Top cap: tooth_count triangles
        // - Bottom cap: tooth_count triangles
        // - Side faces: total_segments quads = total_segments * 6 indices
        const uint numberIndices = total_segments * 3 + total_segments * 3 + total_segments * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("SolidGear", numberVertices, numberIndices))
            return nullptr;

        const float half_thickness = thickness * 0.5f;
        const float angle_per_tooth = (2.0f * math::pi) / float(tooth_count);

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helper function to calculate radius at a given position around the gear
        auto get_radius_at_angle = [&](float angle) -> float
        {
            // Normalize angle to [0, angle_per_tooth)
            float tooth_angle = fmod(angle, angle_per_tooth);
            float tooth_phase = tooth_angle / angle_per_tooth;

            // Tooth profile:
            // 0.0 - tooth_width_ratio/2: transition up to outer radius
            // tooth_width_ratio/2 - (1-tooth_width_ratio/2): tooth top at outer radius
            // (1-tooth_width_ratio/2) - 1.0: transition down to root radius

            float half_width = tooth_width_ratio * 0.5f;
            
            if(tooth_phase < half_width)
            {
                // Rising edge
                float t = tooth_phase / half_width;
                return root_r + (outer_r - root_r) * t;
            }
            else if(tooth_phase < (1.0f - half_width))
            {
                // Tooth top
                return outer_r;
            }
            else
            {
                // Falling edge
                float t = (tooth_phase - (1.0f - half_width)) / half_width;
                return outer_r - (outer_r - root_r) * t;
            }
        };

        // Generate cap centers
        // Top center
        builder.WriteFullVertex(0.0f, 0.0f, half_thickness,
                               0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Top cap ring
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * math::pi;
            float r = get_radius_at_angle(angle);
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            builder.WriteFullVertex(x, y, half_thickness,
                                   0.0f, 0.0f, 1.0f,
                                   -sin(angle), -cos(angle), 0.0f,
                                   x / (outer_r * 2.0f) + 0.5f, -y / (outer_r * 2.0f) + 0.5f);
        }

        // Bottom center
        builder.WriteFullVertex(0.0f, 0.0f, -half_thickness,
                               0.0f, 0.0f, -1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Bottom cap ring
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * math::pi;
            float r = get_radius_at_angle(angle);
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   0.0f, 0.0f, -1.0f,
                                   sin(angle), cos(angle), 0.0f,
                                   x / (outer_r * 2.0f) + 0.5f, -y / (outer_r * 2.0f) + 0.5f);
        }

        // Side vertices
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * math::pi;
            float r = get_radius_at_angle(angle);
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            // Normal pointing outward
            float nx = cos(angle);
            float ny = -sin(angle);
            float nz = 0.0f;

            // Tangent
            float tx = -sin(angle);
            float ty = -cos(angle);
            float tz = 0.0f;

            // Bottom vertex
            builder.WriteFullVertex(x, y, -half_thickness,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(total_segments), 0.0f);

            // Top vertex
            builder.WriteFullVertex(x, y, half_thickness,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(total_segments), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Top cap
            IndexT center_top = 0;
            IndexT ring_start_top = 1;
            
            for(uint i = 0; i < total_segments; i++)
            {
                *ip++ = center_top;
                *ip++ = ring_start_top + i + 1;
                *ip++ = ring_start_top + i;
            }

            // Bottom cap
            IndexT center_bottom = 1 + ring_verts;
            IndexT ring_start_bottom = center_bottom + 1;

            for(uint i = 0; i < total_segments; i++)
            {
                *ip++ = center_bottom;
                *ip++ = ring_start_bottom + i;
                *ip++ = ring_start_bottom + i + 1;
            }

            // Side faces
            IndexT side_base = 1 + ring_verts + 1 + ring_verts;

            for(uint i = 0; i < total_segments; i++)
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
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, -half_thickness),
                       Vector3f(outer_r, outer_r, half_thickness));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
