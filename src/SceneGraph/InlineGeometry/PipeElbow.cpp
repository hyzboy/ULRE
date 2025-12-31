// Pipe elbow geometry generator for ULRE engine
// Creates a curved pipe segment with inner and outer radii

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreatePipeElbow(GeometryCreater *pc, const PipeElbowCreateInfo *peci)
    {
        if(!pc || !peci)
            return nullptr;

        const uint pipe_segs = std::max<uint>(3, peci->pipe_segments);
        const uint bend_segs = std::max<uint>(2, peci->bend_segments);
        const float r_inner = peci->inner_radius;
        const float r_outer = peci->outer_radius;
        const float bend_r = peci->bend_radius;
        const float bend_angle = peci->bend_angle;

        // Validate parameters
        if(r_inner >= r_outer)
            return nullptr;
        
        if(bend_angle <= 0.0f || bend_angle > 180.0f)
            return nullptr;

        // Vertex count:
        // - Path has (bend_segs + 1) positions
        // - Each position has a ring of (pipe_segs + 1) vertices for outer and inner
        // - Total: (bend_segs + 1) * (pipe_segs + 1) * 2
        // - Caps (if enabled): 2 * (pipe_segs + 1) * 2 (two rings per cap)
        
        uint ring_verts = (pipe_segs + 1);
        uint path_rings = (bend_segs + 1);
        uint numberVertices = path_rings * ring_verts * 2;  // outer + inner
        
        uint cap_verts = 0;
        if(peci->generate_caps)
            cap_verts = 2 * ring_verts * 2;  // 2 caps, each with 2 rings
        
        numberVertices += cap_verts;

        // Index count:
        // - Outer surface: bend_segs * pipe_segs * 6
        // - Inner surface: bend_segs * pipe_segs * 6
        // - Caps: 2 * pipe_segs * 6 (if enabled)
        uint numberIndices = bend_segs * pipe_segs * 6 * 2;
        if(peci->generate_caps)
            numberIndices += 2 * pipe_segs * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("PipeElbow", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float bend_angle_rad = deg2rad(bend_angle);
        const float pipe_angle_step = (2.0f * math::pi) / float(pipe_segs);
        const float bend_angle_step = bend_angle_rad / float(bend_segs);

        // Pipe cross-section is a ring (annulus)
        // We'll sweep this ring along a circular arc

        // Generate vertices along the bend path
        // The bend is in the XZ plane, starting from -X direction, rotating towards -Z
        
        for(uint b = 0; b <= bend_segs; b++)
        {
            float t = float(b) / float(bend_segs);  // 0 to 1
            float bend_a = bend_angle_step * float(b);  // Current angle along bend
            
            // Center of pipe at this bend position
            float cx = -bend_r * cos(bend_a);
            float cz = -bend_r * sin(bend_a);
            float cy = 0.0f;

            // Tangent direction along bend path (derivative of position)
            float tx = bend_r * sin(bend_a);
            float tz = -bend_r * cos(bend_a);
            float ty = 0.0f;
            float tlen = sqrtf(tx*tx + tz*tz);
            if(tlen > 0.0f) { tx /= tlen; tz /= tlen; }

            // Normal direction (pointing towards center of bend, in XZ plane)
            float nx = -cos(bend_a);
            float nz = -sin(bend_a);
            float ny = 0.0f;

            // Binormal (perpendicular to tangent and normal)
            // Since tangent is in XZ plane and normal is in XZ plane,
            // binormal should be Y-axis
            float bx = 0.0f;
            float by = 1.0f;
            float bz = 0.0f;

            // Generate ring vertices at this position
            // Outer ring
            for(uint p = 0; p <= pipe_segs; p++)
            {
                float pipe_a = pipe_angle_step * float(p);
                float cos_pa = cos(pipe_a);
                float sin_pa = sin(pipe_a);

                // Offset in ring plane (normal + binormal)
                float offset_x = cos_pa * nx + sin_pa * bx;
                float offset_y = cos_pa * ny + sin_pa * by;
                float offset_z = cos_pa * nz + sin_pa * bz;

                // Position
                float px = cx + offset_x * r_outer;
                float py = cy + offset_y * r_outer;
                float pz = cz + offset_z * r_outer;

                // Normal for outer surface (pointing outward)
                float norm_x = offset_x;
                float norm_y = offset_y;
                float norm_z = offset_z;

                // Tangent along pipe circumference
                float tang_x = -sin_pa * nx + cos_pa * bx;
                float tang_y = -sin_pa * ny + cos_pa * by;
                float tang_z = -sin_pa * nz + cos_pa * bz;

                builder.WriteFullVertex(px, py, pz,
                                       norm_x, norm_y, norm_z,
                                       tang_x, tang_y, tang_z,
                                       t, float(p) / float(pipe_segs));
            }

            // Inner ring
            for(uint p = 0; p <= pipe_segs; p++)
            {
                float pipe_a = pipe_angle_step * float(p);
                float cos_pa = cos(pipe_a);
                float sin_pa = sin(pipe_a);

                float offset_x = cos_pa * nx + sin_pa * bx;
                float offset_y = cos_pa * ny + sin_pa * by;
                float offset_z = cos_pa * nz + sin_pa * bz;

                float px = cx + offset_x * r_inner;
                float py = cy + offset_y * r_inner;
                float pz = cz + offset_z * r_inner;

                // Normal for inner surface (pointing inward)
                float norm_x = -offset_x;
                float norm_y = -offset_y;
                float norm_z = -offset_z;

                float tang_x = -sin_pa * nx + cos_pa * bx;
                float tang_y = -sin_pa * ny + cos_pa * by;
                float tang_z = -sin_pa * nz + cos_pa * bz;

                builder.WriteFullVertex(px, py, pz,
                                       norm_x, norm_y, norm_z,
                                       tang_x, tang_y, tang_z,
                                       t, float(p) / float(pipe_segs));
            }
        }

        uint base_cap_vertex = path_rings * ring_verts * 2;

        // Generate cap vertices if needed
        if(peci->generate_caps)
        {
            // Start cap (at bend angle = 0)
            {
                float bend_a = 0.0f;
                float cx = -bend_r * cos(bend_a);
                float cz = -bend_r * sin(bend_a);
                float cy = 0.0f;

                // Cap normal (facing outward from bend start, which is +X direction)
                float cap_nx = 1.0f;
                float cap_ny = 0.0f;
                float cap_nz = 0.0f;

                float nx = -cos(bend_a);
                float nz = -sin(bend_a);
                float bx = 0.0f;
                float by = 1.0f;
                float bz = 0.0f;

                for(uint p = 0; p <= pipe_segs; p++)
                {
                    float pipe_a = pipe_angle_step * float(p);
                    float cos_pa = cos(pipe_a);
                    float sin_pa = sin(pipe_a);

                    float offset_x = cos_pa * nx + sin_pa * bx;
                    float offset_y = cos_pa * ny + sin_pa * by;
                    float offset_z = cos_pa * nz + sin_pa * bz;

                    // Outer ring
                    float px = cx + offset_x * r_outer;
                    float py = cy + offset_y * r_outer;
                    float pz = cz + offset_z * r_outer;

                    builder.WriteFullVertex(px, py, pz,
                                           cap_nx, cap_ny, cap_nz,
                                           0.0f, 1.0f, 0.0f,
                                           0.5f + offset_x * 0.5f, 0.5f + offset_y * 0.5f);

                    // Inner ring
                    px = cx + offset_x * r_inner;
                    py = cy + offset_y * r_inner;
                    pz = cz + offset_z * r_inner;

                    builder.WriteFullVertex(px, py, pz,
                                           cap_nx, cap_ny, cap_nz,
                                           0.0f, 1.0f, 0.0f,
                                           0.5f + offset_x * 0.3f, 0.5f + offset_y * 0.3f);
                }
            }

            // End cap (at bend angle = bend_angle_rad)
            {
                float bend_a = bend_angle_rad;
                float cx = -bend_r * cos(bend_a);
                float cz = -bend_r * sin(bend_a);
                float cy = 0.0f;

                // Cap normal (facing outward from bend end)
                float cap_nx = -sin(bend_a);
                float cap_ny = 0.0f;
                float cap_nz = cos(bend_a);

                float nx = -cos(bend_a);
                float nz = -sin(bend_a);
                float bx = 0.0f;
                float by = 1.0f;
                float bz = 0.0f;

                for(uint p = 0; p <= pipe_segs; p++)
                {
                    float pipe_a = pipe_angle_step * float(p);
                    float cos_pa = cos(pipe_a);
                    float sin_pa = sin(pipe_a);

                    float offset_x = cos_pa * nx + sin_pa * bx;
                    float offset_y = cos_pa * ny + sin_pa * by;
                    float offset_z = cos_pa * nz + sin_pa * bz;

                    // Outer ring
                    float px = cx + offset_x * r_outer;
                    float py = cy + offset_y * r_outer;
                    float pz = cz + offset_z * r_outer;

                    builder.WriteFullVertex(px, py, pz,
                                           cap_nx, cap_ny, cap_nz,
                                           0.0f, 1.0f, 0.0f,
                                           0.5f + offset_x * 0.5f, 0.5f + offset_y * 0.5f);

                    // Inner ring
                    px = cx + offset_x * r_inner;
                    py = cy + offset_y * r_inner;
                    pz = cz + offset_z * r_inner;

                    builder.WriteFullVertex(px, py, pz,
                                           cap_nx, cap_ny, cap_nz,
                                           0.0f, 1.0f, 0.0f,
                                           0.5f + offset_x * 0.3f, 0.5f + offset_y * 0.3f);
                }
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Outer surface
            for(uint b = 0; b < bend_segs; b++)
            {
                for(uint p = 0; p < pipe_segs; p++)
                {
                    IndexT base = b * ring_verts * 2;  // Start of this bend segment
                    IndexT v0 = base + p;
                    IndexT v1 = base + p + 1;
                    IndexT v2 = base + ring_verts * 2 + p;
                    IndexT v3 = base + ring_verts * 2 + p + 1;

                    *ip++ = v0;
                    *ip++ = v2;
                    *ip++ = v1;

                    *ip++ = v1;
                    *ip++ = v2;
                    *ip++ = v3;
                }
            }

            // Inner surface
            for(uint b = 0; b < bend_segs; b++)
            {
                for(uint p = 0; p < pipe_segs; p++)
                {
                    IndexT base = b * ring_verts * 2 + ring_verts;  // Inner ring offset
                    IndexT v0 = base + p;
                    IndexT v1 = base + p + 1;
                    IndexT v2 = base + ring_verts * 2 + p;
                    IndexT v3 = base + ring_verts * 2 + p + 1;

                    // Reverse winding for inner surface
                    *ip++ = v0;
                    *ip++ = v1;
                    *ip++ = v2;

                    *ip++ = v1;
                    *ip++ = v3;
                    *ip++ = v2;
                }
            }

            // Caps
            if(peci->generate_caps)
            {
                // Start cap
                IndexT cap_base = base_cap_vertex;
                for(uint p = 0; p < pipe_segs; p++)
                {
                    IndexT outer = cap_base + p * 2;
                    IndexT inner = cap_base + p * 2 + 1;
                    IndexT outer_next = cap_base + (p + 1) * 2;
                    IndexT inner_next = cap_base + (p + 1) * 2 + 1;

                    // Triangle facing outward (+X direction)
                    *ip++ = outer;
                    *ip++ = inner;
                    *ip++ = outer_next;

                    *ip++ = outer_next;
                    *ip++ = inner;
                    *ip++ = inner_next;
                }

                // End cap
                cap_base = base_cap_vertex + ring_verts * 2;
                for(uint p = 0; p < pipe_segs; p++)
                {
                    IndexT outer = cap_base + p * 2;
                    IndexT inner = cap_base + p * 2 + 1;
                    IndexT outer_next = cap_base + (p + 1) * 2;
                    IndexT inner_next = cap_base + (p + 1) * 2 + 1;

                    // Triangle facing outward (reverse winding)
                    *ip++ = outer;
                    *ip++ = outer_next;
                    *ip++ = inner;

                    *ip++ = outer_next;
                    *ip++ = inner_next;
                    *ip++ = inner;
                }
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
        // The pipe bends in the XZ plane starting from (-bend_r, 0, 0)
        // Endpoint: (-bend_r*cos(angle), 0, -bend_r*sin(angle))
        BoundingVolumes bv;
        
        float min_x = -(bend_r + r_outer);  // Most negative X (at start)
        float max_x = 0.0f;                 // Most positive X
        float min_y = -r_outer;             // Pipe extends ±r_outer in Y
        float max_y = r_outer;
        
        // Z depends on bend angle
        float end_z = -bend_r * sin(bend_angle_rad);
        float min_z = std::min(0.0f, end_z) - r_outer;
        float max_z = std::max(0.0f, end_z) + r_outer;
        
        bv.SetFromAABB(math::Vector3f(min_x, min_y, min_z),
                       Vector3f(max_x, max_y, max_z));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
