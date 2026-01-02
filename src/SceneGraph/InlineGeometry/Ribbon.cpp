// Ribbon geometry generator for ULRE engine
// Creates a twisted/waving ribbon strip

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateRibbon(GeometryCreater *pc, const RibbonCreateInfo *rci)
    {
        if(!pc || !rci)
            return nullptr;

        const float length = rci->length;
        const float width = rci->width;
        const float thickness = rci->thickness;
        const uint segments = std::max<uint>(2, rci->segments);
        const float twist_turns = rci->twist_turns;
        const float wave_amplitude = rci->wave_amplitude;
        const float wave_frequency = rci->wave_frequency;

        // Validate parameters
        if(length <= 0.0f || width <= 0.0f || thickness <= 0.0f)
            return nullptr;

        const float half_width = width * 0.5f;
        const float half_thickness = thickness * 0.5f;

        // Ribbon structure:
        // - Top surface: (segments + 1) * 2 vertices (left and right edge)
        // - Bottom surface: (segments + 1) * 2 vertices
        // - Left edge: (segments + 1) * 2 vertices (top and bottom)
        // - Right edge: (segments + 1) * 2 vertices (top and bottom)
        // - Front cap: 4 vertices
        // - Back cap: 4 vertices

        const uint verts_per_row = segments + 1;
        const uint top_verts = verts_per_row * 2;
        const uint bottom_verts = verts_per_row * 2;
        const uint left_verts = verts_per_row * 2;
        const uint right_verts = verts_per_row * 2;
        const uint cap_verts = 8; // 4 per cap
        const uint numberVertices = top_verts + bottom_verts + left_verts + right_verts + cap_verts;

        // Indices
        const uint top_indices = segments * 6;
        const uint bottom_indices = segments * 6;
        const uint left_indices = segments * 6;
        const uint right_indices = segments * 6;
        const uint cap_indices = 12; // 6 per cap
        const uint numberIndices = top_indices + bottom_indices + left_indices + right_indices + cap_indices;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Ribbon", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helper function to calculate ribbon path and frame at position t (0 to 1)
        auto calc_ribbon_frame = [&](float t) -> std::tuple<Vector3f, Vector3f, Vector3f, Vector3f>
        {
            // Position along length (X axis)
            float x = (t - 0.5f) * length;
            
            // Wave in Y direction
            float y = wave_amplitude * sin(t * wave_frequency * 2.0f * std::numbers::pi_v<float>);
            float z = 0.0f;
            
            // Tangent (derivative of position)
            float dx = 1.0f;
            float dy = wave_amplitude * wave_frequency * 2.0f * std::numbers::pi_v<float> * cos(t * wave_frequency * 2.0f * std::numbers::pi_v<float>);
            float t_len = sqrtf(dx * dx + dy * dy);
            Vector3f tangent(dx / t_len, dy / t_len, 0.0f);
            
            // Twist angle
            float twist_angle = twist_turns * 2.0f * std::numbers::pi_v<float> * t;
            
            // Initial normal (up) and binormal (right)
            Vector3f initial_normal(0.0f, 0.0f, 1.0f);
            Vector3f initial_binormal(0.0f, 1.0f, 0.0f);
            
            // Rotate normal and binormal around tangent
            float cos_twist = cos(twist_angle);
            float sin_twist = sin(twist_angle);
            
            // Apply twist rotation
            Vector3f normal(
                initial_normal.x * cos_twist - initial_binormal.x * sin_twist,
                initial_normal.y * cos_twist - initial_binormal.y * sin_twist,
                initial_normal.z * cos_twist - initial_binormal.z * sin_twist
            );
            
            // Cross product: binormal = tangent × normal
            Vector3f binormal(
                tangent.y * normal.z - tangent.z * normal.y,
                tangent.z * normal.x - tangent.x * normal.z,
                tangent.x * normal.y - tangent.y * normal.x
            );
            
            // Normalize binormal
            float b_len = sqrtf(binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z);
            binormal.x /= b_len;
            binormal.y /= b_len;
            binormal.z /= b_len;
            
            // Reorthogonalize normal = binormal × tangent
            normal.x = binormal.y * tangent.z - binormal.z * tangent.y;
            normal.y = binormal.z * tangent.x - binormal.x * tangent.z;
            normal.z = binormal.x * tangent.y - binormal.y * tangent.x;
            
            return {Vector3f(x, y, z), tangent, normal, binormal};
        };

        // Generate top surface vertices
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            // Left edge of top surface
            float v_left_x = pos.x - binormal.x * half_width + normal.x * half_thickness;
            float v_left_y = pos.y - binormal.y * half_width + normal.y * half_thickness;
            float v_left_z = pos.z - binormal.z * half_width + normal.z * half_thickness;
            
            builder.WriteFullVertex(v_left_x, v_left_y, v_left_z,
                                   normal.x, normal.y, normal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   0.0f, t);
            
            // Right edge of top surface
            float v_right_x = pos.x + binormal.x * half_width + normal.x * half_thickness;
            float v_right_y = pos.y + binormal.y * half_width + normal.y * half_thickness;
            float v_right_z = pos.z + binormal.z * half_width + normal.z * half_thickness;
            
            builder.WriteFullVertex(v_right_x, v_right_y, v_right_z,
                                   normal.x, normal.y, normal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   1.0f, t);
        }

        // Generate bottom surface vertices
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            // Left edge of bottom surface
            float v_left_x = pos.x - binormal.x * half_width - normal.x * half_thickness;
            float v_left_y = pos.y - binormal.y * half_width - normal.y * half_thickness;
            float v_left_z = pos.z - binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v_left_x, v_left_y, v_left_z,
                                   -normal.x, -normal.y, -normal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   0.0f, t);
            
            // Right edge of bottom surface
            float v_right_x = pos.x + binormal.x * half_width - normal.x * half_thickness;
            float v_right_y = pos.y + binormal.y * half_width - normal.y * half_thickness;
            float v_right_z = pos.z + binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v_right_x, v_right_y, v_right_z,
                                   -normal.x, -normal.y, -normal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   1.0f, t);
        }

        // Generate left edge vertices
        uint left_base = top_verts + bottom_verts;
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            // Top edge of left side
            float v_top_x = pos.x - binormal.x * half_width + normal.x * half_thickness;
            float v_top_y = pos.y - binormal.y * half_width + normal.y * half_thickness;
            float v_top_z = pos.z - binormal.z * half_width + normal.z * half_thickness;
            
            builder.WriteFullVertex(v_top_x, v_top_y, v_top_z,
                                   -binormal.x, -binormal.y, -binormal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   t, 1.0f);
            
            // Bottom edge of left side
            float v_bottom_x = pos.x - binormal.x * half_width - normal.x * half_thickness;
            float v_bottom_y = pos.y - binormal.y * half_width - normal.y * half_thickness;
            float v_bottom_z = pos.z - binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v_bottom_x, v_bottom_y, v_bottom_z,
                                   -binormal.x, -binormal.y, -binormal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   t, 0.0f);
        }

        // Generate right edge vertices
        uint right_base = left_base + left_verts;
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            // Top edge of right side
            float v_top_x = pos.x + binormal.x * half_width + normal.x * half_thickness;
            float v_top_y = pos.y + binormal.y * half_width + normal.y * half_thickness;
            float v_top_z = pos.z + binormal.z * half_width + normal.z * half_thickness;
            
            builder.WriteFullVertex(v_top_x, v_top_y, v_top_z,
                                   binormal.x, binormal.y, binormal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   t, 1.0f);
            
            // Bottom edge of right side
            float v_bottom_x = pos.x + binormal.x * half_width - normal.x * half_thickness;
            float v_bottom_y = pos.y + binormal.y * half_width - normal.y * half_thickness;
            float v_bottom_z = pos.z + binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v_bottom_x, v_bottom_y, v_bottom_z,
                                   binormal.x, binormal.y, binormal.z,
                                   tangent.x, tangent.y, tangent.z,
                                   t, 0.0f);
        }

        // Generate front cap vertices (t=0)
        uint cap_base = right_base + right_verts;
        {
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(0.0f);
            
            float v0_x = pos.x - binormal.x * half_width + normal.x * half_thickness;
            float v0_y = pos.y - binormal.y * half_width + normal.y * half_thickness;
            float v0_z = pos.z - binormal.z * half_width + normal.z * half_thickness;
            
            float v1_x = pos.x + binormal.x * half_width + normal.x * half_thickness;
            float v1_y = pos.y + binormal.y * half_width + normal.y * half_thickness;
            float v1_z = pos.z + binormal.z * half_width + normal.z * half_thickness;
            
            float v2_x = pos.x + binormal.x * half_width - normal.x * half_thickness;
            float v2_y = pos.y + binormal.y * half_width - normal.y * half_thickness;
            float v2_z = pos.z + binormal.z * half_width - normal.z * half_thickness;
            
            float v3_x = pos.x - binormal.x * half_width - normal.x * half_thickness;
            float v3_y = pos.y - binormal.y * half_width - normal.y * half_thickness;
            float v3_z = pos.z - binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v0_x, v0_y, v0_z, -tangent.x, -tangent.y, -tangent.z, binormal.x, binormal.y, binormal.z, 0.0f, 1.0f);
            builder.WriteFullVertex(v1_x, v1_y, v1_z, -tangent.x, -tangent.y, -tangent.z, binormal.x, binormal.y, binormal.z, 1.0f, 1.0f);
            builder.WriteFullVertex(v2_x, v2_y, v2_z, -tangent.x, -tangent.y, -tangent.z, binormal.x, binormal.y, binormal.z, 1.0f, 0.0f);
            builder.WriteFullVertex(v3_x, v3_y, v3_z, -tangent.x, -tangent.y, -tangent.z, binormal.x, binormal.y, binormal.z, 0.0f, 0.0f);
        }

        // Generate back cap vertices (t=1)
        {
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(1.0f);
            
            float v0_x = pos.x - binormal.x * half_width + normal.x * half_thickness;
            float v0_y = pos.y - binormal.y * half_width + normal.y * half_thickness;
            float v0_z = pos.z - binormal.z * half_width + normal.z * half_thickness;
            
            float v1_x = pos.x + binormal.x * half_width + normal.x * half_thickness;
            float v1_y = pos.y + binormal.y * half_width + normal.y * half_thickness;
            float v1_z = pos.z + binormal.z * half_width + normal.z * half_thickness;
            
            float v2_x = pos.x + binormal.x * half_width - normal.x * half_thickness;
            float v2_y = pos.y + binormal.y * half_width - normal.y * half_thickness;
            float v2_z = pos.z + binormal.z * half_width - normal.z * half_thickness;
            
            float v3_x = pos.x - binormal.x * half_width - normal.x * half_thickness;
            float v3_y = pos.y - binormal.y * half_width - normal.y * half_thickness;
            float v3_z = pos.z - binormal.z * half_width - normal.z * half_thickness;
            
            builder.WriteFullVertex(v0_x, v0_y, v0_z, tangent.x, tangent.y, tangent.z, binormal.x, binormal.y, binormal.z, 0.0f, 1.0f);
            builder.WriteFullVertex(v1_x, v1_y, v1_z, tangent.x, tangent.y, tangent.z, binormal.x, binormal.y, binormal.z, 1.0f, 1.0f);
            builder.WriteFullVertex(v2_x, v2_y, v2_z, tangent.x, tangent.y, tangent.z, binormal.x, binormal.y, binormal.z, 1.0f, 0.0f);
            builder.WriteFullVertex(v3_x, v3_y, v3_z, tangent.x, tangent.y, tangent.z, binormal.x, binormal.y, binormal.z, 0.0f, 0.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;
            
            // Top surface
            for(uint i = 0; i < segments; i++)
            {
                IndexT base = i * 2;
                *ip++ = base + 0; *ip++ = base + 2; *ip++ = base + 1;
                *ip++ = base + 1; *ip++ = base + 2; *ip++ = base + 3;
            }
            
            // Bottom surface
            IndexT bottom_base = top_verts;
            for(uint i = 0; i < segments; i++)
            {
                IndexT base = bottom_base + i * 2;
                *ip++ = base + 0; *ip++ = base + 1; *ip++ = base + 2;
                *ip++ = base + 1; *ip++ = base + 3; *ip++ = base + 2;
            }
            
            // Left edge
            for(uint i = 0; i < segments; i++)
            {
                IndexT base = left_base + i * 2;
                *ip++ = base + 0; *ip++ = base + 1; *ip++ = base + 2;
                *ip++ = base + 1; *ip++ = base + 3; *ip++ = base + 2;
            }
            
            // Right edge
            for(uint i = 0; i < segments; i++)
            {
                IndexT base = right_base + i * 2;
                *ip++ = base + 0; *ip++ = base + 2; *ip++ = base + 1;
                *ip++ = base + 1; *ip++ = base + 2; *ip++ = base + 3;
            }
            
            // Front cap
            *ip++ = cap_base + 0; *ip++ = cap_base + 1; *ip++ = cap_base + 2;
            *ip++ = cap_base + 0; *ip++ = cap_base + 2; *ip++ = cap_base + 3;
            
            // Back cap
            *ip++ = cap_base + 4; *ip++ = cap_base + 6; *ip++ = cap_base + 5;
            *ip++ = cap_base + 4; *ip++ = cap_base + 7; *ip++ = cap_base + 6;
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

        // Set bounding box (conservative estimate)
        Geometry *p = pc->Create();
        if(p)
        {
            float max_y = wave_amplitude + half_width;
            float max_z = half_width + half_thickness;
            BoundingVolumes bv;
            bv.SetFromAABB(Vector3f(-length * 0.5f, -max_y, -max_z),
                          Vector3f(length * 0.5f, max_y, max_z));
            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}//namespace hgl::graph::inline_geometry
