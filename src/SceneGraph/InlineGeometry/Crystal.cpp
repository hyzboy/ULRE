// Crystal geometry generator for ULRE engine
// Creates a pointed crystal with polygonal base

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCrystal(GeometryCreater *pc, const CrystalCreateInfo *cci)
    {
        if(!pc || !cci)
            return nullptr;

        const float base_radius = cci->base_radius;
        const float height = cci->height;
        const float tip_sharpness = std::min(std::max(cci->tip_sharpness, 0.001f), 1.0f);
        const uint sides = std::max<uint>(3, cci->sides);
        const uint segments = std::max<uint>(1, cci->segments);
        const float irregularity = std::min(std::max(cci->irregularity, 0.0f), 0.5f);

        // Validate parameters
        if(base_radius <= 0.0f || height <= 0.0f)
            return nullptr;

        // Crystal structure:
        // - Base (polygon at y=0)
        // - Tapered body (segments rings from base to near-tip)
        // - Tip point (y=height)

        const uint rings = segments + 1; // including base
        const uint profile_verts_per_ring = sides + 1; // close the loop

        // Vertices:
        // - Base center: 1
        // - Base ring: sides
        // - Body rings: (rings-1) * sides
        // - Tip point: 1
        // - Side faces need duplicate vertices for proper normals
        
        // For smooth shading along height but flat faces on sides:
        // - Base cap: 1 center + sides vertices
        // - Side strips: sides * rings vertices
        // - Tip: 1 vertex
        
        const uint base_cap_verts = 1 + sides;
        const uint side_verts = sides * (rings + 1); // including tip as last ring
        const uint numberVertices = base_cap_verts + side_verts;

        // Indices:
        // - Base cap: sides triangles
        // - Side faces: sides * segments quads + sides triangles for tip
        const uint base_indices = sides * 3;
        const uint side_quad_indices = sides * (rings - 1) * 6;
        const uint tip_indices = sides * 3;
        const uint numberIndices = base_indices + side_quad_indices + tip_indices;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Crystal", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(sides);
        const float tip_radius = base_radius * tip_sharpness;

        // Simple hash function for pseudo-random irregularity
        auto hash_float = [](uint seed) -> float
        {
            seed = (seed ^ 61) ^ (seed >> 16);
            seed = seed + (seed << 3);
            seed = seed ^ (seed >> 4);
            seed = seed * 0x27d4eb2d;
            seed = seed ^ (seed >> 15);
            return float(seed % 10000) / 10000.0f;
        };

        // Base cap center
        builder.WriteFullVertex(0.0f, 0.0f, 0.0f,
                               0.0f, -1.0f, 0.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Base cap ring
        for(uint i = 0; i < sides; i++)
        {
            float angle = angle_step * float(i);
            float x = cos(angle) * base_radius;
            float z = -sin(angle) * base_radius;
            
            // Add irregularity
            float random_factor = hash_float(i * 137 + 42);
            float radius_adjust = 1.0f + (random_factor - 0.5f) * irregularity * 2.0f;
            x *= radius_adjust;
            z *= radius_adjust;
            
            builder.WriteFullVertex(x, 0.0f, z,
                                   0.0f, -1.0f, 0.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.5f + cos(angle) * 0.5f, 0.5f + sin(angle) * 0.5f);
        }

        // Side vertices (including base and tip rings)
        for(uint ring = 0; ring <= rings; ring++)
        {
            float t = float(ring) / float(rings);
            float y = height * t;
            
            // Radius tapers from base_radius to tip_radius
            float radius = base_radius * (1.0f - t) + tip_radius * t;
            
            for(uint i = 0; i < sides; i++)
            {
                float angle = angle_step * float(i);
                float next_angle = angle_step * float(i + 1);
                
                // Current and next positions
                float cos_a = cos(angle);
                float sin_a = -sin(angle);
                float cos_next = cos(next_angle);
                float sin_next = -sin(next_angle);
                
                // Add irregularity
                float random_factor = hash_float(i * 137 + ring * 73);
                float radius_adjust = 1.0f + (random_factor - 0.5f) * irregularity * 2.0f;
                float r = radius * radius_adjust;
                
                float x = cos_a * r;
                float z = sin_a * r;
                
                // Calculate face normal (average of adjacent face normals)
                float p0_x = x, p0_y = y, p0_z = z;
                
                // Get neighbor points for normal calculation
                float r_next = radius * (1.0f + (hash_float((i+1) * 137 + ring * 73) - 0.5f) * irregularity * 2.0f);
                float p1_x = cos_next * r_next;
                float p1_y = y;
                float p1_z = sin_next * r_next;
                
                float y_up = (ring < rings) ? height * float(ring + 1) / float(rings) : y;
                float r_up = (ring < rings) ? (base_radius * (1.0f - t - 1.0f/rings) + tip_radius * (t + 1.0f/rings)) * radius_adjust : 0.0f;
                float p2_x = cos_a * r_up;
                float p2_y = y_up;
                float p2_z = sin_a * r_up;
                
                // Calculate outward normal (edge1 × edge2)
                float edge1_x = p1_x - p0_x;
                float edge1_y = p1_y - p0_y;
                float edge1_z = p1_z - p0_z;
                
                float edge2_x = p2_x - p0_x;
                float edge2_y = p2_y - p0_y;
                float edge2_z = p2_z - p0_z;
                
                float normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                float normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                float normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                // Normalize
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                float u = float(i) / float(sides);
                float v = 1.0f - t;
                
                builder.WriteFullVertex(x, y, z,
                                       normal_x, normal_y, normal_z,
                                       1.0f, 0.0f, 0.0f,
                                       u, v);
            }
        }

        // Generate indices
        {
            const IndexType index_type = pc->GetIndexType();
            
            auto generate_indices = [&](auto *ip)
            {
                // Base cap
                for(uint i = 0; i < sides; i++)
                {
                    *ip++ = 0;                      // center
                    *ip++ = 1 + i;                  // current
                    *ip++ = 1 + (i + 1) % sides;    // next
                }
                
                // Side quads
                uint side_base = base_cap_verts;
                for(uint ring = 0; ring < rings - 1; ring++)
                {
                    for(uint i = 0; i < sides; i++)
                    {
                        uint current = side_base + ring * sides + i;
                        uint next = side_base + ring * sides + (i + 1) % sides;
                        uint current_up = side_base + (ring + 1) * sides + i;
                        uint next_up = side_base + (ring + 1) * sides + (i + 1) % sides;
                        
                        // Quad as two triangles
                        *ip++ = current;
                        *ip++ = current_up;
                        *ip++ = next;
                        
                        *ip++ = next;
                        *ip++ = current_up;
                        *ip++ = next_up;
                    }
                }
                
                // Tip triangles
                uint last_ring_base = side_base + (rings - 1) * sides;
                uint tip_ring_base = side_base + rings * sides;
                for(uint i = 0; i < sides; i++)
                {
                    *ip++ = last_ring_base + i;
                    *ip++ = tip_ring_base + i;
                    *ip++ = last_ring_base + (i + 1) % sides;
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
        }

        // Create geometry and set bounding box
        float max_radius = base_radius * (1.0f + irregularity);
        
        return pc->CreateWithAABB(
            Vector3f(-max_radius, 0.0f, -max_radius),
            Vector3f(max_radius, height, max_radius));
    }
}//namespace hgl::graph::inline_geometry
