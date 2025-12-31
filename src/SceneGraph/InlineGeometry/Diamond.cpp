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
        const float top_ratio = clamp(dci->top_ratio, 0.01f, 0.99f);
        const float table_ratio = clamp(dci->table_ratio, 0.0f, 1.0f);
        const float pavilion_ratio = clamp(dci->pavilion_ratio, 0.0f, 1.0f);
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

        const float angle_step = (2.0f * math::pi) / float(facets);

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
                Vector3f v0(0.0f, top_height, 0.0f);
                Vector3f v1(table_radius * cos1, top_height, table_radius * sin1);
                Vector3f v2(table_radius * cos2, top_height, table_radius * sin2);
                
                Vector3f normal(0.0f, 1.0f, 0.0f); // Flat top
                
                builder.WriteFullVertex(v0, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f, 0.5f);
                builder.WriteFullVertex(v1, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f + cos1*0.5f, 0.5f + sin1*0.5f);
                builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f + cos2*0.5f, 0.5f + sin2*0.5f);

                // Triangle from table edge to girdle
                Vector3f v3 = v1;
                Vector3f v4 = v2;
                Vector3f v5(radius * cos1, 0.0f, radius * sin1);
                Vector3f v6(radius * cos2, 0.0f, radius * sin2);
                
                // Calculate facet normal
                Vector3f edge1 = v5 - v3;
                Vector3f edge2 = v6 - v3;
                Vector3f facet_normal = normalize(cross(edge1, edge2));
                
                builder.WriteFullVertex(v3, facet_normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 1.0f);
                builder.WriteFullVertex(v6, facet_normal, Vector3f(1.0f, 0.0f, 0.0f), float(i+1)/float(facets), 0.0f);
                builder.WriteFullVertex(v5, facet_normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.0f);
            }
            else
            {
                // Triangle from top point to girdle
                Vector3f v0(0.0f, top_height, 0.0f);
                Vector3f v1(radius * cos1, 0.0f, radius * sin1);
                Vector3f v2(radius * cos2, 0.0f, radius * sin2);
                
                Vector3f edge1 = v1 - v0;
                Vector3f edge2 = v2 - v0;
                Vector3f normal = normalize(cross(edge1, edge2));
                
                builder.WriteFullVertex(v0, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f, 1.0f);
                builder.WriteFullVertex(v1, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i+1)/float(facets), 0.5f);
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
                Vector3f v0(radius * cos1, 0.0f, radius * sin1);
                Vector3f v1(radius * cos2, 0.0f, radius * sin2);
                Vector3f v2(pavilion_bottom_radius * cos1, -bottom_height * 0.7f, pavilion_bottom_radius * sin1);
                Vector3f v3(pavilion_bottom_radius * cos2, -bottom_height * 0.7f, pavilion_bottom_radius * sin2);
                
                Vector3f edge1 = v2 - v0;
                Vector3f edge2 = v1 - v0;
                Vector3f normal = normalize(cross(edge1, edge2));
                
                builder.WriteFullVertex(v0, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v1, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i+1)/float(facets), 0.5f);
                builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.0f);

                // Triangle from pavilion base to culet
                Vector3f v4(0.0f, -bottom_height, 0.0f);
                
                edge1 = v3 - v2;
                edge2 = v4 - v2;
                normal = normalize(cross(edge1, edge2));
                
                builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.0f);
                builder.WriteFullVertex(v3, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i+1)/float(facets), 0.0f);
                builder.WriteFullVertex(v4, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f, 0.0f);
            }
            else
            {
                // Triangle from girdle directly to culet point
                Vector3f v0(radius * cos1, 0.0f, radius * sin1);
                Vector3f v1(radius * cos2, 0.0f, radius * sin2);
                Vector3f v2(0.0f, -bottom_height, 0.0f);
                
                Vector3f edge1 = v2 - v0;
                Vector3f edge2 = v1 - v0;
                Vector3f normal = normalize(cross(edge1, edge2));
                
                builder.WriteFullVertex(v0, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i)/float(facets), 0.5f);
                builder.WriteFullVertex(v1, normal, Vector3f(1.0f, 0.0f, 0.0f), float(i+1)/float(facets), 0.5f);
                builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.5f, 0.0f);
            }
        }

        // Generate indices (simple sequential)
        if(!IndexGenerator::CreateSequentialIndices(pc, numberIndices))
            return nullptr;

        // Set bounding box
        Geometry *p = pc->End();
        if(p)
        {
            BoundingVolumes bv;
            bv.SetFromAABB(Vector3f(-radius, -bottom_height, -radius),
                          Vector3f(radius, top_height, radius));
            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}//namespace hgl::graph::inline_geometry
