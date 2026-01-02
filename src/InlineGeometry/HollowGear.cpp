// Hollow gear geometry generator for ULRE engine
// Creates a cylindrical gear with teeth around the perimeter and a circular hole in the center

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateHollowGear(GeometryCreater *pc, const HollowGearCreateInfo *hgci)
    {
        if(!pc || !hgci)
            return nullptr;

        const uint tooth_count = std::max<uint>(3, hgci->tooth_count);
        const uint segs_per_tooth = std::max<uint>(1, hgci->segments_per_tooth);
        const float outer_r = hgci->outer_radius;
        const float root_r = hgci->root_radius;
        const float inner_r = hgci->inner_radius;
        const float thickness = hgci->thickness;
        const float tooth_width_ratio = std::clamp(hgci->tooth_width_ratio, 0.1f, 0.9f);

        // Validate parameters
        if(root_r >= outer_r || inner_r >= root_r)
            return nullptr;

        if(thickness <= 0.0f || inner_r <= 0.0f)
            return nullptr;

        // Total segments around the gear
        const uint total_segments = tooth_count * segs_per_tooth * 4;
        const uint inner_segments = total_segments;  // Match outer for simplicity

        // Vertices:
        // - Top face: outer ring + inner ring
        // - Bottom face: outer ring + inner ring
        // - Outer side: outer ring (top and bottom)
        // - Inner side: inner ring (top and bottom)
        const uint ring_verts = total_segments + 1;
        const uint inner_ring_verts = inner_segments + 1;
        const uint numberVertices = ring_verts * 2 + inner_ring_verts * 2 + ring_verts * 2 + inner_ring_verts * 2;

        // Indices:
        // - Top cap: total_segments annular quads = total_segments * 6
        // - Bottom cap: total_segments annular quads = total_segments * 6
        // - Outer side: total_segments quads = total_segments * 6
        // - Inner side: inner_segments quads = inner_segments * 6
        const uint numberIndices = total_segments * 6 + total_segments * 6 + total_segments * 6 + inner_segments * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("HollowGear", numberVertices, numberIndices))
            return nullptr;

        const float half_thickness = thickness * 0.5f;
        const float angle_per_tooth = (2.0f * std::numbers::pi_v<float>) / float(tooth_count);

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helper function to calculate radius at a given position around the gear
        auto get_radius_at_angle = [&](float angle) -> float
        {
            float tooth_angle = fmod(angle, angle_per_tooth);
            float tooth_phase = tooth_angle / angle_per_tooth;

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

        // Top face - outer ring
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * std::numbers::pi_v<float>;
            float r = get_radius_at_angle(angle);
            // Note: Y uses negative sine to match ULRE's coordinate system
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            // UV mapping: outer ring normalized to [0,1] then scaled to fit texture
            // Simple radial mapping: center at (0.5, 0.5), radius scales to 0.5
            float u = 0.5f + (x / outer_r) * 0.5f;
            float v = 0.5f + (y / outer_r) * 0.5f;

            builder.WriteFullVertex(x, y, half_thickness,
                                   0.0f, 0.0f, 1.0f,
                                   -sin(angle), -cos(angle), 0.0f,
                                   u, v);
        }

        // Top face - inner ring
        for(uint i = 0; i <= inner_segments; i++)
        {
            float angle = (float(i) / float(inner_segments)) * 2.0f * std::numbers::pi_v<float>;
            float x = cos(angle) * inner_r;
            float y = -sin(angle) * inner_r;

            // UV mapping: inner ring also uses radial mapping from center
            float u = 0.5f + (x / outer_r) * 0.5f;
            float v = 0.5f + (y / outer_r) * 0.5f;

            builder.WriteFullVertex(x, y, half_thickness,
                                   0.0f, 0.0f, 1.0f,
                                   -sin(angle), -cos(angle), 0.0f,
                                   u, v);
        }

        // Bottom face - outer ring
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * std::numbers::pi_v<float>;
            float r = get_radius_at_angle(angle);
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            float u = 0.5f + (x / outer_r) * 0.5f;
            float v = 0.5f + (y / outer_r) * 0.5f;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   0.0f, 0.0f, -1.0f,
                                   sin(angle), cos(angle), 0.0f,
                                   u, v);
        }

        // Bottom face - inner ring
        for(uint i = 0; i <= inner_segments; i++)
        {
            float angle = (float(i) / float(inner_segments)) * 2.0f * std::numbers::pi_v<float>;
            float x = cos(angle) * inner_r;
            float y = -sin(angle) * inner_r;

            float u = 0.5f + (x / outer_r) * 0.5f;
            float v = 0.5f + (y / outer_r) * 0.5f;

            builder.WriteFullVertex(x, y, -half_thickness,
                                   0.0f, 0.0f, -1.0f,
                                   sin(angle), cos(angle), 0.0f,
                                   u, v);
        }

        // Outer side vertices
        for(uint i = 0; i <= total_segments; i++)
        {
            float angle = (float(i) / float(total_segments)) * 2.0f * std::numbers::pi_v<float>;
            float r = get_radius_at_angle(angle);
            float x = cos(angle) * r;
            float y = -sin(angle) * r;

            float nx = cos(angle);
            float ny = -sin(angle);
            float nz = 0.0f;

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

        // Inner side vertices (normals pointing inward)
        for(uint i = 0; i <= inner_segments; i++)
        {
            float angle = (float(i) / float(inner_segments)) * 2.0f * std::numbers::pi_v<float>;
            float x = cos(angle) * inner_r;
            float y = -sin(angle) * inner_r;

            // Normal pointing inward (towards center)
            // For inward normals: negate both X and Y components of outward normal
            // Outward normal: (cos(angle), -sin(angle), 0)
            // Inward normal: (-cos(angle), sin(angle), 0)
            float nx = -cos(angle);
            float ny = sin(angle);
            float nz = 0.0f;

            float tx = -sin(angle);
            float ty = -cos(angle);
            float tz = 0.0f;

            // Bottom vertex
            builder.WriteFullVertex(x, y, -half_thickness,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(inner_segments), 0.0f);

            // Top vertex
            builder.WriteFullVertex(x, y, half_thickness,
                                   nx, ny, nz,
                                   tx, ty, tz,
                                   float(i) / float(inner_segments), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Top cap (annular)
            IndexT outer_top = 0;
            IndexT inner_top = ring_verts;

            for(uint i = 0; i < total_segments; i++)
            {
                IndexT o0 = outer_top + i;
                IndexT o1 = outer_top + i + 1;
                IndexT i0 = inner_top + i;
                IndexT i1 = inner_top + i + 1;

                *ip++ = o0;
                *ip++ = o1;
                *ip++ = i0;

                *ip++ = i0;
                *ip++ = o1;
                *ip++ = i1;
            }

            // Bottom cap (annular)
            IndexT outer_bottom = ring_verts + inner_ring_verts;
            IndexT inner_bottom = outer_bottom + ring_verts;

            for(uint i = 0; i < total_segments; i++)
            {
                IndexT o0 = outer_bottom + i;
                IndexT o1 = outer_bottom + i + 1;
                IndexT i0 = inner_bottom + i;
                IndexT i1 = inner_bottom + i + 1;

                *ip++ = o0;
                *ip++ = i0;
                *ip++ = o1;

                *ip++ = i0;
                *ip++ = i1;
                *ip++ = o1;
            }

            // Outer side
            IndexT outer_side = ring_verts + inner_ring_verts + ring_verts + inner_ring_verts;

            for(uint i = 0; i < total_segments; i++)
            {
                IndexT v0 = outer_side + i * 2;
                IndexT v1 = outer_side + i * 2 + 1;
                IndexT v2 = outer_side + (i + 1) * 2;
                IndexT v3 = outer_side + (i + 1) * 2 + 1;

                *ip++ = v0;
                *ip++ = v1;
                *ip++ = v2;

                *ip++ = v2;
                *ip++ = v1;
                *ip++ = v3;
            }

            // Inner side (reverse winding)
            IndexT inner_side = outer_side + ring_verts * 2;

            for(uint i = 0; i < inner_segments; i++)
            {
                IndexT v0 = inner_side + i * 2;
                IndexT v1 = inner_side + i * 2 + 1;
                IndexT v2 = inner_side + (i + 1) * 2;
                IndexT v3 = inner_side + (i + 1) * 2 + 1;

                *ip++ = v0;
                *ip++ = v2;
                *ip++ = v1;

                *ip++ = v2;
                *ip++ = v3;
                *ip++ = v1;
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
            math::Vector3f(-outer_r, -outer_r, -half_thickness),
            Vector3f(outer_r, outer_r, half_thickness));
    }
} // namespace hgl::graph::inline_geometry
