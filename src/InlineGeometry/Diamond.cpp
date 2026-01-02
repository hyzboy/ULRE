// Diamond geometry generator for ULRE engine
// Creates a faceted diamond/gem shape with configurable proportions

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateDiamond(GeometryCreater *pc, const DiamondCreateInfo *dci)
    {
        if(!pc || !dci)
            return nullptr;

        const float radius = dci->radius;
        const float height = dci->height;
        const float top_ratio = std::min(std::max(dci->top_ratio, 0.01f), 0.99f);
        const float table_ratio = std::min(std::max(dci->table_ratio, 0.0f), 1.0f);
        const float pavilion_ratio = std::min(std::max(dci->pavilion_ratio, 0.0f), 1.0f);
        const uint facets = std::max<uint>(3, dci->facets);

        // Validate parameters
        if(radius <= 0.0f || height <= 0.0f)
            return nullptr;

        // Diamond structure from top to bottom:
        // 1. Table (flat top) - optional if table_ratio > 0
        // 2. Crown (upper part) - from table to girdle
        // 3. Girdle (widest part at middle)
        // 4. Pavilion (lower part) - from girdle to culet
        // 5. Culet (bottom point or small flat)

        const float top_height = height * top_ratio;
        const float bottom_height = height * (1.0f - top_ratio);
        const float table_radius = radius * table_ratio;
        const float pavilion_bottom_radius = radius * pavilion_ratio;

        // Vertex counts:
        // - Table center (if table_ratio > 0): 1
        // - Table ring: facets (if table_ratio > 0)
        // - Girdle ring: facets
        // - Pavilion bottom: facets (if pavilion_ratio > 0) or 1 point
        // Each facet needs separate vertices for flat shading
        
        const bool has_table = (table_ratio > 0.001f);
        const bool has_pavilion_base = (pavilion_ratio > 0.001f);

        // For flat faceted look, each triangle needs its own vertices
        // Crown: facets * 3 vertices per triangle
        // Pavilion: facets * 3 vertices per triangle
        const uint crown_triangles = has_table ? facets * 2 : facets; // table to girdle
        const uint pavilion_triangles = has_pavilion_base ? facets * 2 : facets; // girdle to culet
        
        const uint numberVertices = (crown_triangles + pavilion_triangles) * 3;
        const uint numberIndices = (crown_triangles + pavilion_triangles) * 3;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Diamond", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(facets);

        // Crown facets (top part)
        for(uint i = 0; i < facets; i++)
        {
            float angle1 = angle_step * float(i);
            float angle2 = angle_step * float(i + 1);
            
            float cos1 = cos(angle1);
            float sin1 = -sin(angle1);
            float cos2 = cos(angle2);
            float sin2 = -sin(angle2);

            if(has_table)
            {
                // Triangle from table center to table edge
                float v0_x = 0.0f, v0_y = top_height, v0_z = 0.0f;
                float v1_x = table_radius * cos1, v1_y = top_height, v1_z = table_radius * sin1;
                float v2_x = table_radius * cos2, v2_y = top_height, v2_z = table_radius * sin2;
                
                float normal_x = 0.0f, normal_y = 1.0f, normal_z = 0.0f; // Flat top
                
                builder.WriteFullVertex(v0_x, v0_y, v0_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f);
                builder.WriteFullVertex(v1_x, v1_y, v1_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f + cos1*0.5f, 0.5f + sin1*0.5f);
                builder.WriteFullVertex(v2_x, v2_y, v2_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f + cos2*0.5f, 0.5f + sin2*0.5f);

                // Triangle from table edge to girdle
                float v3_x = v1_x, v3_y = v1_y, v3_z = v1_z;
                float v4_x = v2_x, v4_y = v2_y, v4_z = v2_z;
                float v5_x = radius * cos1, v5_y = 0.0f, v5_z = radius * sin1;
                float v6_x = radius * cos2, v6_y = 0.0f, v6_z = radius * sin2;
                
                // Calculate facet normal
                float edge1_x = v5_x - v3_x;
                float edge1_y = v5_y - v3_y;
                float edge1_z = v5_z - v3_z;
                
                float edge2_x = v6_x - v3_x;
                float edge2_y = v6_y - v3_y;
                float edge2_z = v6_z - v3_z;
                
                // Cross product
                float facet_normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                float facet_normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                float facet_normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                // Normalize
                float facet_normal_len = sqrtf(facet_normal_x * facet_normal_x + facet_normal_y * facet_normal_y + facet_normal_z * facet_normal_z);
                if(facet_normal_len > 0.0001f)
                {
                    facet_normal_x /= facet_normal_len;
                    facet_normal_y /= facet_normal_len;
                    facet_normal_z /= facet_normal_len;
                }
                
                builder.WriteFullVertex(v3_x, v3_y, v3_z, facet_normal_x, facet_normal_y, facet_normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 1.0f);
                builder.WriteFullVertex(v6_x, v6_y, v6_z, facet_normal_x, facet_normal_y, facet_normal_z, 1.0f, 0.0f, 0.0f, float(i+1)/float(facets), 0.0f);
                builder.WriteFullVertex(v5_x, v5_y, v5_z, facet_normal_x, facet_normal_y, facet_normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.0f);
            }
            else
            {
                // Triangle from top point to girdle
                float v0_x = 0.0f, v0_y = top_height, v0_z = 0.0f;
                float v1_x = radius * cos1, v1_y = 0.0f, v1_z = radius * sin1;
                float v2_x = radius * cos2, v2_y = 0.0f, v2_z = radius * sin2;
                
                float edge1_x = v1_x - v0_x;
                float edge1_y = v1_y - v0_y;
                float edge1_z = v1_z - v0_z;
                
                float edge2_x = v2_x - v0_x;
                float edge2_y = v2_y - v0_y;
                float edge2_z = v2_z - v0_z;
                
                float normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                float normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                float normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                builder.WriteFullVertex(v0_x, v0_y, v0_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f);
                builder.WriteFullVertex(v1_x, v1_y, v1_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v2_x, v2_y, v2_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i+1)/float(facets), 0.5f);
            }
        }

        // Pavilion facets (bottom part)
        for(uint i = 0; i < facets; i++)
        {
            float angle1 = angle_step * float(i);
            float angle2 = angle_step * float(i + 1);
            
            float cos1 = cos(angle1);
            float sin1 = -sin(angle1);
            float cos2 = cos(angle2);
            float sin2 = -sin(angle2);

            if(has_pavilion_base)
            {
                // Triangle from girdle to pavilion base
                float v0_x = radius * cos1, v0_y = 0.0f, v0_z = radius * sin1;
                float v1_x = radius * cos2, v1_y = 0.0f, v1_z = radius * sin2;
                float v2_x = pavilion_bottom_radius * cos1, v2_y = -bottom_height * 0.7f, v2_z = pavilion_bottom_radius * sin1;
                float v3_x = pavilion_bottom_radius * cos2, v3_y = -bottom_height * 0.7f, v3_z = pavilion_bottom_radius * sin2;
                
                float edge1_x = v2_x - v0_x;
                float edge1_y = v2_y - v0_y;
                float edge1_z = v2_z - v0_z;
                
                float edge2_x = v1_x - v0_x;
                float edge2_y = v1_y - v0_y;
                float edge2_z = v1_z - v0_z;
                
                float normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                float normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                float normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                builder.WriteFullVertex(v0_x, v0_y, v0_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v1_x, v1_y, v1_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i+1)/float(facets), 0.5f);
                builder.WriteFullVertex(v2_x, v2_y, v2_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.0f);

                // Triangle from pavilion base to culet
                float v4_x = 0.0f, v4_y = -bottom_height, v4_z = 0.0f;
                
                edge1_x = v3_x - v2_x;
                edge1_y = v3_y - v2_y;
                edge1_z = v3_z - v2_z;
                
                edge2_x = v4_x - v2_x;
                edge2_y = v4_y - v2_y;
                edge2_z = v4_z - v2_z;
                
                normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                builder.WriteFullVertex(v2_x, v2_y, v2_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.0f);
                builder.WriteFullVertex(v3_x, v3_y, v3_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i+1)/float(facets), 0.0f);
                builder.WriteFullVertex(v4_x, v4_y, v4_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);
            }
            else
            {
                // Triangle from girdle directly to culet point
                float v0_x = radius * cos1, v0_y = 0.0f, v0_z = radius * sin1;
                float v1_x = radius * cos2, v1_y = 0.0f, v1_z = radius * sin2;
                float v2_x = 0.0f, v2_y = -bottom_height, v2_z = 0.0f;
                
                float edge1_x = v2_x - v0_x;
                float edge1_y = v2_y - v0_y;
                float edge1_z = v2_z - v0_z;
                
                float edge2_x = v1_x - v0_x;
                float edge2_y = v1_y - v0_y;
                float edge2_z = v1_z - v0_z;
                
                float normal_x = edge1_y * edge2_z - edge1_z * edge2_y;
                float normal_y = edge1_z * edge2_x - edge1_x * edge2_z;
                float normal_z = edge1_x * edge2_y - edge1_y * edge2_x;
                
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }
                
                builder.WriteFullVertex(v0_x, v0_y, v0_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v1_x, v1_y, v1_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, float(i+1)/float(facets), 0.5f);
                builder.WriteFullVertex(v2_x, v2_y, v2_z, normal_x, normal_y, normal_z, 1.0f, 0.0f, 0.0f, 0.5f, 0.0f);
            }
        }

        // Generate indices (simple sequential - 0, 1, 2, 3, 4, 5, ...)
        {
            const IndexType index_type = pc->GetIndexType();
            
            if(index_type == IndexType::U16)
            {
                IBTypeMap<uint16> ib(pc->GetIBMap());
                uint16 *ip = ib;
                for(uint i = 0; i < numberIndices; i++)
                    *ip++ = (uint16)i;
            }
            else if(index_type == IndexType::U32)
            {
                IBTypeMap<uint32> ib(pc->GetIBMap());
                uint32 *ip = ib;
                for(uint i = 0; i < numberIndices; i++)
                    *ip++ = i;
            }
            else if(index_type == IndexType::U8)
            {
                IBTypeMap<uint8> ib(pc->GetIBMap());
                uint8 *ip = ib;
                for(uint i = 0; i < numberIndices; i++)
                    *ip++ = (uint8)i;
            }
            else
                return nullptr;
        }

        // Set bounding box
        return pc->CreateWithAABB(
            Vector3f(-radius, -bottom_height, -radius),
            Vector3f(radius, top_height, radius));
    }
}//namespace hgl::graph::inline_geometry
