// Helix (spring/spiral) geometry generator for ULRE engine
// Creates a helical coil with optional solid tube or wireframe

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateHelix(GeometryCreater *pc, const HelixCreateInfo *hci)
    {
        if(!pc || !hci)
            return nullptr;

        const float radius = hci->radius;
        const float height = hci->height;
        const float coil_r = hci->coil_radius;
        const uint turns = std::max<uint>(1, hci->turns);
        const uint segs_per_turn = std::max<uint>(4, hci->segments_per_turn);
        const uint coil_segs = std::max<uint>(3, hci->coil_segments);
        const bool solid = hci->solid;

        // Total segments along helix path
        const uint total_segs = turns * segs_per_turn;

        uint numberVertices = 0;
        uint numberIndices = 0;

        if(solid)
        {
            // Solid tube: sweep circular cross-section along helix
            // Vertices: (total_segs + 1) path points * (coil_segs + 1) circle points
            numberVertices = (total_segs + 1) * (coil_segs + 1);
            // Indices: total_segs * coil_segs * 6 (triangles)
            numberIndices = total_segs * coil_segs * 6;
        }
        else
        {
            // Wireframe: just the center line
            // Note: This generates GL_LINES indices (pairs of vertex indices)
            // The geometry should be rendered with line primitive topology
            numberVertices = total_segs + 1;
            numberIndices = total_segs * 2;  // Each line segment uses 2 indices
        }

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Helix", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helix parametric equations:
        // x = radius * cos(t)
        // y = radius * sin(t)
        // z = (height / (2*pi*turns)) * t
        // where t goes from 0 to 2*pi*turns

        const float total_angle = 2.0f * math::pi * float(turns);
        const float angle_step = total_angle / float(total_segs);
        const float z_per_angle = height / total_angle;

        if(solid)
        {
            // Generate solid tube
            const float coil_angle_step = (2.0f * math::pi) / float(coil_segs);

            for(uint s = 0; s <= total_segs; s++)
            {
                float t = angle_step * float(s);
                
                // Center point of coil at this position
                float center_x = radius * cos(t);
                float center_y = radius * sin(t);
                float center_z = z_per_angle * t;

                // Tangent to helix (derivative of position)
                float dx_dt = -radius * sin(t);
                float dy_dt = radius * cos(t);
                float dz_dt = z_per_angle;

                // Normalize tangent
                float tang_len = sqrtf(dx_dt*dx_dt + dy_dt*dy_dt + dz_dt*dz_dt);
                if(tang_len > 0.0f)
                {
                    dx_dt /= tang_len;
                    dy_dt /= tang_len;
                    dz_dt /= tang_len;
                }

                // Normal vector (perpendicular to tangent, pointing towards helix axis)
                // Using Frenet frame: N = T' / |T'| (curvature direction)
                // For circular helix, normal points towards axis
                float norm_x = -cos(t);
                float norm_y = -sin(t);
                float norm_z = 0.0f;

                // Binormal (perpendicular to both tangent and normal)
                float binorm_x = dy_dt * norm_z - dz_dt * norm_y;
                float binorm_y = dz_dt * norm_x - dx_dt * norm_z;
                float binorm_z = dx_dt * norm_y - dy_dt * norm_x;

                // Normalize binormal
                float binorm_len = sqrtf(binorm_x*binorm_x + binorm_y*binorm_y + binorm_z*binorm_z);
                if(binorm_len > 0.0f)
                {
                    binorm_x /= binorm_len;
                    binorm_y /= binorm_len;
                    binorm_z /= binorm_len;
                }

                // Generate circle vertices around helix path
                for(uint c = 0; c <= coil_segs; c++)
                {
                    float coil_a = coil_angle_step * float(c);
                    float cos_ca = cos(coil_a);
                    float sin_ca = sin(coil_a);

                    // Offset in the plane perpendicular to tangent
                    float offset_x = cos_ca * norm_x + sin_ca * binorm_x;
                    float offset_y = cos_ca * norm_y + sin_ca * binorm_y;
                    float offset_z = cos_ca * norm_z + sin_ca * binorm_z;

                    // Vertex position
                    float px = center_x + offset_x * coil_r;
                    float py = center_y + offset_y * coil_r;
                    float pz = center_z + offset_z * coil_r;

                    // Normal (pointing outward from tube center)
                    float nx = offset_x;
                    float ny = offset_y;
                    float nz = offset_z;

                    // Tangent along tube surface (along helix path)
                    float tx = dx_dt;
                    float ty = dy_dt;
                    float tz = dz_dt;

                    // UV coordinates
                    float u = float(s) / float(total_segs);
                    float v = float(c) / float(coil_segs);

                    builder.WriteFullVertex(px, py, pz, nx, ny, nz, tx, ty, tz, u, v);
                }
            }
        }
        else
        {
            // Generate wireframe (center line only)
            for(uint s = 0; s <= total_segs; s++)
            {
                float t = angle_step * float(s);
                
                float px = radius * cos(t);
                float py = radius * sin(t);
                float pz = z_per_angle * t;

                // Tangent to helix (derivative of position)
                float dx_dt = -radius * sin(t);
                float dy_dt = radius * cos(t);
                float dz_dt = z_per_angle;

                // Normalize tangent
                float tang_len = sqrtf(dx_dt*dx_dt + dy_dt*dy_dt + dz_dt*dz_dt);
                float tx = (tang_len > 0.0f) ? dx_dt / tang_len : 0.0f;
                float ty = (tang_len > 0.0f) ? dy_dt / tang_len : 0.0f;
                float tz = (tang_len > 0.0f) ? dz_dt / tang_len : 0.0f;

                // Normal (pointing towards axis)
                float nx = -cos(t);
                float ny = -sin(t);
                float nz = 0.0f;

                float u = float(s) / float(total_segs);

                builder.WriteFullVertex(px, py, pz, nx, ny, nz, tx, ty, tz, u, 0.0f);
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            if(solid)
            {
                // Tube surface indices
                for(uint s = 0; s < total_segs; s++)
                {
                    for(uint c = 0; c < coil_segs; c++)
                    {
                        IndexT v0 = s * (coil_segs + 1) + c;
                        IndexT v1 = s * (coil_segs + 1) + c + 1;
                        IndexT v2 = (s + 1) * (coil_segs + 1) + c;
                        IndexT v3 = (s + 1) * (coil_segs + 1) + c + 1;

                        *ip++ = v0;
                        *ip++ = v2;
                        *ip++ = v1;

                        *ip++ = v1;
                        *ip++ = v2;
                        *ip++ = v3;
                    }
                }
            }
            else
            {
                // Line indices
                for(uint s = 0; s < total_segs; s++)
                {
                    *ip++ = (IndexT)s;
                    *ip++ = (IndexT)(s + 1);
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
        BoundingVolumes bv;
        float max_radius = radius + coil_r;
        
        bv.SetFromAABB(math::Vector3f(-max_radius, -max_radius, 0.0f),
                       Vector3f(max_radius, max_radius, height));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
