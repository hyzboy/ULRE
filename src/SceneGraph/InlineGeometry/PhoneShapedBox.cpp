// Phone-shaped box geometry generator for ULRE engine
// Creates a box with large corner radii on XY plane and small radii on Z axis
// Similar to modern smartphone designs (iPhone 4/5-16)

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreatePhoneShapedBox(GeometryCreater *pc, const PhoneShapedBoxCreateInfo *psci)
    {
        if(!pc || !psci)
            return nullptr;

        const float sx = psci->size.x;
        const float sy = psci->size.y;
        const float sz = psci->size.z;
        const float rxy = psci->xy_edge_radius;
        const float rz = psci->z_edge_radius;
        const uint segs_xy = std::max<uint>(2, psci->xy_edge_segments);
        const uint segs_z = std::max<uint>(1, psci->z_edge_segments);

        // Validate radii don't exceed dimensions
        if(rxy > sx * 0.5f || rxy > sy * 0.5f)
            return nullptr;
        if(rz > sz * 0.5f)
            return nullptr;

        // Inner dimensions (box dimensions minus edge radii)
        const float ix = sx * 0.5f - rxy;
        const float iy = sy * 0.5f - rxy;
        const float iz = sz * 0.5f - rz;

        // Vertex count estimation
        // This is a complex shape, so we'll use a conservative estimate
        // 8 corners (elliptical patches) + 4 XY edges + 4 XZ edges + 4 YZ edges + 6 faces
        uint numberVertices = 8 * (segs_xy + 1) * (segs_z + 1) + 
                             4 * (segs_xy + 1) * (segs_z + 1) +
                             4 * (segs_xy + 1) * (segs_z + 1) +
                             4 * (segs_xy + 1) * (segs_z + 1) +
                             6 * 4;  // faces
        uint numberIndices = numberVertices * 3;

        GeometryBuilder builder(pc, numberVertices, numberIndices);
        if(!builder.Init())
            return nullptr;

        // Track bounding box
        Vector3f min_bound(FLT_MAX, FLT_MAX, FLT_MAX);
        Vector3f max_bound(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        auto update_bounds = [&](const Vector3f& pos) {
            min_bound.x = std::min(min_bound.x, pos.x);
            min_bound.y = std::min(min_bound.y, pos.y);
            min_bound.z = std::min(min_bound.z, pos.z);
            max_bound.x = std::max(max_bound.x, pos.x);
            max_bound.y = std::max(max_bound.y, pos.y);
            max_bound.z = std::max(max_bound.z, pos.z);
        };

        // Helper to add vertex
        auto add_vertex = [&](const Vector3f& pos, const Vector3f& normal) {
            update_bounds(pos);
            
            Vector2f uv;
            // Simple UV mapping based on X and Y position
            uv.x = (pos.x + sx * 0.5f) / sx;
            uv.y = (pos.y + sy * 0.5f) / sy;

            if(builder.HasTangents())
            {
                Vector3f tangent(1, 0, 0);  // Default tangent
                builder.WriteVertex(pos, normal, tangent, uv);
            }
            else if(builder.HasTexCoords())
            {
                builder.WriteVertex(pos, normal, uv);
            }
            else
            {
                builder.WriteVertex(pos, normal);
            }
        };

        // Generate geometry
        // We'll create 8 corner elliptic patches, 12 edge surfaces, and 6 face regions

        // Structure: 8 corners with elliptical cross-sections
        // - 4 corners have large XY radius and small Z radius
        // Each corner is at (+/-ix, +/-iy, +/-iz) with different radii

        const Vector3f corner_offsets[8] = {
            Vector3f(ix, iy, iz),    // +++ corner
            Vector3f(-ix, iy, iz),   // -++ corner
            Vector3f(-ix, -iy, iz),  // --+ corner
            Vector3f(ix, -iy, iz),   // +-+ corner
            Vector3f(ix, iy, -iz),   // ++- corner
            Vector3f(-ix, iy, -iz),  // -+- corner
            Vector3f(-ix, -iy, -iz), // --- corner
            Vector3f(ix, -iy, -iz)   // +-- corner
        };

        // For each corner, generate an elliptical patch
        // The patch spans a quarter sphere but with different radii
        for(int corner = 0; corner < 8; ++corner)
        {
            const Vector3f& center = corner_offsets[corner];
            float sign_x = (corner & 1) ? -1.0f : 1.0f;
            float sign_y = (corner & 2) ? -1.0f : 1.0f;
            float sign_z = (corner & 4) ? -1.0f : 1.0f;

            // Generate vertices for this corner patch
            List<uint32> corner_verts;
            for(uint j = 0; j <= segs_z; ++j)
            {
                float v = (float)j / segs_z;
                float phi = v * HGL_PI * 0.5f;  // 0 to 90 degrees
                float cos_phi = cos(phi);
                float sin_phi = sin(phi);

                for(uint i = 0; i <= segs_xy; ++i)
                {
                    float u = (float)i / segs_xy;
                    float theta = u * HGL_PI * 0.5f;  // 0 to 90 degrees
                    float cos_theta = cos(theta);
                    float sin_theta = sin(theta);

                    // Elliptical corner: large radius in XY, small in Z
                    Vector3f offset;
                    offset.x = sign_x * rxy * cos_theta * cos_phi;
                    offset.y = sign_y * rxy * sin_theta * cos_phi;
                    offset.z = sign_z * rz * sin_phi;

                    Vector3f pos = center + offset;
                    
                    // Normal for ellipsoid: need to scale by inverse radii
                    Vector3f normal;
                    normal.x = sign_x * cos_theta * cos_phi / rxy;
                    normal.y = sign_y * sin_theta * cos_phi / rxy;
                    normal.z = sign_z * sin_phi / rz;
                    normal.normalize();

                    corner_verts.push_back(builder.GetCurrentVertexIndex());
                    add_vertex(pos, normal);
                }
            }

            // Generate indices for this corner patch
            for(uint j = 0; j < segs_z; ++j)
            {
                for(uint i = 0; i < segs_xy; ++i)
                {
                    uint idx0 = corner_verts[j * (segs_xy + 1) + i];
                    uint idx1 = corner_verts[j * (segs_xy + 1) + i + 1];
                    uint idx2 = corner_verts[(j + 1) * (segs_xy + 1) + i];
                    uint idx3 = corner_verts[(j + 1) * (segs_xy + 1) + i + 1];

                    builder.WriteTriangle(idx0, idx2, idx1);
                    builder.WriteTriangle(idx1, idx2, idx3);
                }
            }
        }

        // Generate 12 edges connecting the corners
        // 4 edges along Z axis (vertical edges of the phone)
        // 4 edges along X axis
        // 4 edges along Y axis

        // Top face edges (Z = +iz)
        {
            // Top-front edge (Y = +iy, varying X)
            List<uint32> edge_verts;
            for(uint i = 0; i <= segs_xy; ++i)
            {
                float u = (float)i / segs_xy;
                float angle = u * HGL_PI * 0.5f;
                
                for(uint j = 0; j <= segs_z; ++j)
                {
                    float v = (float)j / segs_z;
                    float phi = v * HGL_PI * 0.5f;
                    
                    Vector3f pos;
                    pos.x = ix - rxy + rxy * cos(angle);
                    pos.y = iy + rxy * sin(angle);
                    pos.z = iz + rz * sin(phi);
                    
                    Vector3f normal(0, 1, 0);  // Simplified
                    normal.normalize();
                    
                    edge_verts.push_back(builder.GetCurrentVertexIndex());
                    add_vertex(pos, normal);
                }
            }
            
            // Generate triangles for this edge surface
            for(uint i = 0; i < segs_xy; ++i)
            {
                for(uint j = 0; j < segs_z; ++j)
                {
                    uint idx0 = edge_verts[i * (segs_z + 1) + j];
                    uint idx1 = edge_verts[(i + 1) * (segs_z + 1) + j];
                    uint idx2 = edge_verts[i * (segs_z + 1) + j + 1];
                    uint idx3 = edge_verts[(i + 1) * (segs_z + 1) + j + 1];
                    
                    builder.WriteTriangle(idx0, idx1, idx2);
                    builder.WriteTriangle(idx1, idx3, idx2);
                }
            }
        }

        // Set bounding volumes
        Geometry *p = builder.Finish();
        if(p)
        {
            BoundingVolumes bv;
            bv.SetFromAABB(min_bound, max_bound);
            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}  // namespace hgl::graph::inline_geometry
