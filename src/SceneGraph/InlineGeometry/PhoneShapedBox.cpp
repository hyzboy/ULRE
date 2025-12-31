// Phone-shaped box geometry generator for ULRE engine
// Creates a simplified box with rounded edges
// Note: This is a simplified placeholder implementation

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

        // Simplified: Create a basic rounded box
        // For now, just create a simple box (full implementation would be much more complex)
        
        const float hx = sx * 0.5f;
        const float hy = sy * 0.5f;
        const float hz = sz * 0.5f;

        // Simple box: 8 vertices, 6 faces, 12 triangles
        const uint numberVertices = 24; // 4 vertices per face for proper normals
        const uint numberIndices = 36;  // 6 faces * 2 triangles * 3 indices

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("PhoneShapedBox", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // Define the 6 faces of the box
        // Each face has 4 vertices with proper normals
        
        // Front face (+Z)
        builder.WriteFullVertex(-hx, -hy, hz, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex( hx, -hy, hz, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex( hx,  hy, hz, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex(-hx,  hy, hz, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        // Back face (-Z)
        builder.WriteFullVertex( hx, -hy, -hz, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex(-hx, -hy, -hz, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex(-hx,  hy, -hz, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex( hx,  hy, -hz, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        // Top face (+Y)
        builder.WriteFullVertex(-hx, hy, -hz, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex( hx, hy, -hz, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex( hx, hy,  hz, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex(-hx, hy,  hz, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        // Bottom face (-Y)
        builder.WriteFullVertex(-hx, -hy,  hz, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex( hx, -hy,  hz, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex( hx, -hy, -hz, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex(-hx, -hy, -hz, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

        // Right face (+X)
        builder.WriteFullVertex(hx, -hy,  hz, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex(hx, -hy, -hz, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex(hx,  hy, -hz, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex(hx,  hy,  hz, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);

        // Left face (-X)
        builder.WriteFullVertex(-hx, -hy, -hz, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
        builder.WriteFullVertex(-hx, -hy,  hz, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
        builder.WriteFullVertex(-hx,  hy,  hz, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f);
        builder.WriteFullVertex(-hx,  hy, -hz, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);

        // Generate indices (2 triangles per face)
        {
            const IndexType index_type = pc->GetIndexType();
            
            auto generate_indices = [&](auto *ip)
            {
                for(uint face = 0; face < 6; face++)
                {
                    uint base = face * 4;
                    // First triangle
                    *ip++ = base + 0;
                    *ip++ = base + 1;
                    *ip++ = base + 2;
                    // Second triangle
                    *ip++ = base + 0;
                    *ip++ = base + 2;
                    *ip++ = base + 3;
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

        // Set bounding volumes
        Geometry *p = pc->Create();
        if(p)
        {
            BoundingVolumes bv;
            bv.SetFromAABB(Vector3f(-hx, -hy, -hz), Vector3f(hx, hy, hz));
            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}  // namespace hgl::graph::inline_geometry
