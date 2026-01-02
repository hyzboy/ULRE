// Heart geometry generator for ULRE engine
// Creates a 3D heart shape using parametric equations

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateHeart(GeometryCreater *pc, const HeartCreateInfo *hci)
    {
        if(!pc || !hci)
            return nullptr;

        const uint segments = std::max<uint>(8, hci->segments);
        const uint depth_segs = std::max<uint>(1, hci->depth_segments);
        const float size = hci->size;
        const float depth = hci->depth;
        const float half_depth = depth * 0.5f;

        // Validate parameters
        if(size <= 0.0f || depth <= 0.0f)
            return nullptr;

        // Heart shape parametric equation (2D profile in XY plane):
        // x(t) = 16 * sin^3(t)
        // y(t) = 13*cos(t) - 5*cos(2t) - 2*cos(3t) - cos(4t)
        // where t goes from 0 to 2*pi
        
        // Vertices:
        // - Front face: segments + 1 vertices around heart + center
        // - Back face: segments + 1 vertices around heart + center
        // - Side: (segments + 1) * (depth_segs + 1) vertices for extrusion
        const uint profile_verts = segments + 1;
        const uint front_verts = profile_verts + 1;  // profile + center
        const uint back_verts = profile_verts + 1;   // profile + center
        const uint side_verts = profile_verts * (depth_segs + 1);
        const uint numberVertices = front_verts + back_verts + side_verts;

        // Indices:
        // - Front cap: segments triangles
        // - Back cap: segments triangles
        // - Side: segments * depth_segs quads = segments * depth_segs * 6
        const uint numberIndices = segments * 3 + segments * 3 + segments * depth_segs * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Heart", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helper function to calculate heart shape coordinates
        auto heart_point = [&](float t) -> std::pair<float, float>
        {
            // Parametric heart curve
            float sin_t = sin(t);
            float cos_t = cos(t);
            float sin3_t = sin_t * sin_t * sin_t;
            
            float x = 16.0f * sin3_t;
            float y = 13.0f * cos_t 
                    - 5.0f * cos(2.0f * t) 
                    - 2.0f * cos(3.0f * t) 
                    - cos(4.0f * t);
            
            // Scale and normalize
            x = x * size / 16.0f;
            y = y * size / 16.0f;
            
            return {x, y};
        };

        // Calculate center point (average of all profile points)
        float center_x = 0.0f;
        float center_y = 0.0f;
        for(uint i = 0; i < segments; i++)
        {
            float t = (float(i) / float(segments)) * 2.0f * std::numbers::pi_v<float>;
            auto [x, y] = heart_point(t);
            center_x += x;
            center_y += y;
        }
        center_x /= float(segments);
        center_y /= float(segments);

        // Front face center
        builder.WriteFullVertex(center_x, center_y, half_depth,
                               0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Front face profile
        for(uint i = 0; i <= segments; i++)
        {
            float t = (float(i % segments) / float(segments)) * 2.0f * std::numbers::pi_v<float>;
            auto [x, y] = heart_point(t);

            builder.WriteFullVertex(x, y, half_depth,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   (x / size + 1.0f) * 0.5f, (y / size + 1.0f) * 0.5f);
        }

        // Back face center
        builder.WriteFullVertex(center_x, center_y, -half_depth,
                               0.0f, 0.0f, -1.0f,
                               -1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Back face profile
        for(uint i = 0; i <= segments; i++)
        {
            float t = (float(i % segments) / float(segments)) * 2.0f * std::numbers::pi_v<float>;
            auto [x, y] = heart_point(t);

            builder.WriteFullVertex(x, y, -half_depth,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   (x / size + 1.0f) * 0.5f, (y / size + 1.0f) * 0.5f);
        }

        // Side vertices (extruded profile)
        for(uint d = 0; d <= depth_segs; d++)
        {
            float z_ratio = float(d) / float(depth_segs);
            float z = -half_depth + depth * z_ratio;

            for(uint i = 0; i <= segments; i++)
            {
                float t = (float(i % segments) / float(segments)) * 2.0f * std::numbers::pi_v<float>;
                auto [x, y] = heart_point(t);

                // Calculate tangent (derivative of the curve at this point)
                float dt = 0.01f;  // Small differential for numerical derivative
                float t_prev = t - dt;
                float t_next = t + dt;
                auto [x_prev, y_prev] = heart_point(t_prev);
                auto [x_next, y_next] = heart_point(t_next);
                
                // Tangent along the curve (derivative)
                float tx = (x_next - x_prev) / (2.0f * dt);
                float ty = (y_next - y_prev) / (2.0f * dt);
                float t_len = sqrtf(tx * tx + ty * ty);
                if(t_len > 0.0f) { tx /= t_len; ty /= t_len; }

                // Normal perpendicular to tangent (pointing outward)
                float nx = -ty;
                float ny = tx;
                float nz = 0.0f;

                // Tangent along extrusion direction
                float tang_x = 0.0f;
                float tang_y = 0.0f;
                float tang_z = 1.0f;

                builder.WriteFullVertex(x, y, z,
                                       nx, ny, nz,
                                       tang_x, tang_y, tang_z,
                                       float(i) / float(segments), z_ratio);
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Front face
            IndexT front_center = 0;
            IndexT front_ring_start = 1;

            for(uint i = 0; i < segments; i++)
            {
                *ip++ = front_center;
                *ip++ = front_ring_start + i + 1;
                *ip++ = front_ring_start + i;
            }

            // Back face
            IndexT back_center = front_verts;
            IndexT back_ring_start = back_center + 1;

            for(uint i = 0; i < segments; i++)
            {
                *ip++ = back_center;
                *ip++ = back_ring_start + i;
                *ip++ = back_ring_start + i + 1;
            }

            // Side faces
            IndexT side_base = front_verts + back_verts;

            for(uint d = 0; d < depth_segs; d++)
            {
                for(uint i = 0; i < segments; i++)
                {
                    IndexT v0 = side_base + d * profile_verts + i;
                    IndexT v1 = side_base + d * profile_verts + i + 1;
                    IndexT v2 = side_base + (d + 1) * profile_verts + i;
                    IndexT v3 = side_base + (d + 1) * profile_verts + i + 1;

                    *ip++ = v0;
                    *ip++ = v2;
                    *ip++ = v1;

                    *ip++ = v1;
                    *ip++ = v2;
                    *ip++ = v3;
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

        // Calculate actual bounds from the heart shape
        // The parametric heart curve has specific mathematical bounds:
        // X: approximately ±16 (scaled by size/16)
        // Y: approximately -4 to 17 (scaled by size/16)
        float max_x = size * (16.0f / 16.0f);      // ±1.0 * size
        float max_y = size * (17.0f / 16.0f);      // ~1.06 * size
        float min_y = size * (-4.0f / 16.0f);      // ~-0.25 * size
        
        return pc->CreateWithAABB(
            Vector3f(-max_x, min_y, -half_depth),
            Vector3f(max_x, max_y, half_depth));
    }
} // namespace hgl::graph::inline_geometry
