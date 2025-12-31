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
            float y = wave_amplitude * sin(t * wave_frequency * 2.0f * math::pi);
            float z = 0.0f;
            
            // Tangent (derivative of position)
            float dx = 1.0f;
            float dy = wave_amplitude * wave_frequency * 2.0f * math::pi * cos(t * wave_frequency * 2.0f * math::pi);
            Vector3f tangent = normalize(Vector3f(dx, dy, 0.0f));
            
            // Twist angle
            float twist_angle = twist_turns * 2.0f * math::pi * t;
            
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
            
            Vector3f binormal = cross(tangent, normal);
            normal = cross(binormal, tangent); // Reorthogonalize
            
            return {Vector3f(x, y, z), tangent, normalize(normal), normalize(binormal)};
        };

        // Generate top surface vertices
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            // Left edge of top surface
            Vector3f v_left = pos - binormal * half_width + normal * half_thickness;
            builder.WriteFullVertex(v_left, normal, tangent, 0.0f, t);
            
            // Right edge of top surface
            Vector3f v_right = pos + binormal * half_width + normal * half_thickness;
            builder.WriteFullVertex(v_right, normal, tangent, 1.0f, t);
        }

        // Generate bottom surface vertices
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            Vector3f bottom_normal = -normal;
            
            // Left edge of bottom surface
            Vector3f v_left = pos - binormal * half_width - normal * half_thickness;
            builder.WriteFullVertex(v_left, bottom_normal, tangent, 0.0f, t);
            
            // Right edge of bottom surface
            Vector3f v_right = pos + binormal * half_width - normal * half_thickness;
            builder.WriteFullVertex(v_right, bottom_normal, tangent, 1.0f, t);
        }

        // Generate left edge vertices
        uint left_base = top_verts + bottom_verts;
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            Vector3f left_normal = -binormal;
            
            // Top edge of left side
            Vector3f v_top = pos - binormal * half_width + normal * half_thickness;
            builder.WriteFullVertex(v_top, left_normal, tangent, t, 1.0f);
            
            // Bottom edge of left side
            Vector3f v_bottom = pos - binormal * half_width - normal * half_thickness;
            builder.WriteFullVertex(v_bottom, left_normal, tangent, t, 0.0f);
        }

        // Generate right edge vertices
        uint right_base = left_base + left_verts;
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(t);
            
            Vector3f right_normal = binormal;
            
            // Top edge of right side
            Vector3f v_top = pos + binormal * half_width + normal * half_thickness;
            builder.WriteFullVertex(v_top, right_normal, tangent, t, 1.0f);
            
            // Bottom edge of right side
            Vector3f v_bottom = pos + binormal * half_width - normal * half_thickness;
            builder.WriteFullVertex(v_bottom, right_normal, tangent, t, 0.0f);
        }

        // Generate front cap vertices (t=0)
        uint cap_base = right_base + right_verts;
        {
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(0.0f);
            Vector3f cap_normal = -tangent;
            
            Vector3f v0 = pos - binormal * half_width + normal * half_thickness;
            Vector3f v1 = pos + binormal * half_width + normal * half_thickness;
            Vector3f v2 = pos + binormal * half_width - normal * half_thickness;
            Vector3f v3 = pos - binormal * half_width - normal * half_thickness;
            
            builder.WriteFullVertex(v0, cap_normal, binormal, 0.0f, 1.0f);
            builder.WriteFullVertex(v1, cap_normal, binormal, 1.0f, 1.0f);
            builder.WriteFullVertex(v2, cap_normal, binormal, 1.0f, 0.0f);
            builder.WriteFullVertex(v3, cap_normal, binormal, 0.0f, 0.0f);
        }

        // Generate back cap vertices (t=1)
        {
            auto [pos, tangent, normal, binormal] = calc_ribbon_frame(1.0f);
            Vector3f cap_normal = tangent;
            
            Vector3f v0 = pos - binormal * half_width + normal * half_thickness;
            Vector3f v1 = pos + binormal * half_width + normal * half_thickness;
            Vector3f v2 = pos + binormal * half_width - normal * half_thickness;
            Vector3f v3 = pos - binormal * half_width - normal * half_thickness;
            
            builder.WriteFullVertex(v0, cap_normal, binormal, 0.0f, 1.0f);
            builder.WriteFullVertex(v1, cap_normal, binormal, 1.0f, 1.0f);
            builder.WriteFullVertex(v2, cap_normal, binormal, 1.0f, 0.0f);
            builder.WriteFullVertex(v3, cap_normal, binormal, 0.0f, 0.0f);
        }

        // Generate indices
        uint idx = 0;
        
        // Top surface
        for(uint i = 0; i < segments; i++)
        {
            uint base = i * 2;
            builder.WriteIndex(idx++, base + 0);
            builder.WriteIndex(idx++, base + 2);
            builder.WriteIndex(idx++, base + 1);
            
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 2);
            builder.WriteIndex(idx++, base + 3);
        }
        
        // Bottom surface
        uint bottom_base = top_verts;
        for(uint i = 0; i < segments; i++)
        {
            uint base = bottom_base + i * 2;
            builder.WriteIndex(idx++, base + 0);
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 2);
            
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 3);
            builder.WriteIndex(idx++, base + 2);
        }
        
        // Left edge
        for(uint i = 0; i < segments; i++)
        {
            uint base = left_base + i * 2;
            builder.WriteIndex(idx++, base + 0);
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 2);
            
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 3);
            builder.WriteIndex(idx++, base + 2);
        }
        
        // Right edge
        for(uint i = 0; i < segments; i++)
        {
            uint base = right_base + i * 2;
            builder.WriteIndex(idx++, base + 0);
            builder.WriteIndex(idx++, base + 2);
            builder.WriteIndex(idx++, base + 1);
            
            builder.WriteIndex(idx++, base + 1);
            builder.WriteIndex(idx++, base + 2);
            builder.WriteIndex(idx++, base + 3);
        }
        
        // Front cap
        builder.WriteIndex(idx++, cap_base + 0);
        builder.WriteIndex(idx++, cap_base + 1);
        builder.WriteIndex(idx++, cap_base + 2);
        builder.WriteIndex(idx++, cap_base + 0);
        builder.WriteIndex(idx++, cap_base + 2);
        builder.WriteIndex(idx++, cap_base + 3);
        
        // Back cap
        builder.WriteIndex(idx++, cap_base + 4);
        builder.WriteIndex(idx++, cap_base + 6);
        builder.WriteIndex(idx++, cap_base + 5);
        builder.WriteIndex(idx++, cap_base + 4);
        builder.WriteIndex(idx++, cap_base + 7);
        builder.WriteIndex(idx++, cap_base + 6);

        // Set bounding box (conservative estimate)
        Geometry *p = pc->End();
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
