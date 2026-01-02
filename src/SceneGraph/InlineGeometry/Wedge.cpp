// Wedge/triangular prism geometry generator for ULRE engine
// Creates a wedge shape (half of a box with a sloped surface)

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateWedge(GeometryCreater *pc, const WedgeCreateInfo *wci)
    {
        if(!pc || !wci)
            return nullptr;

        const float width = wci->width;
        const float depth = wci->depth;
        const float height = wci->height;
        const bool slope_x = wci->slope_direction_x;

        // Validate parameters
        if(width <= 0.0f || depth <= 0.0f || height <= 0.0f)
            return nullptr;

        const float hw = width * 0.5f;
        const float hd = depth * 0.5f;
        const float hh = height * 0.5f;

        // Wedge has 5 faces: 2 triangular ends, 1 rectangular bottom, 1 rectangular back, 1 sloped top
        // Total vertices: 6 unique positions, but we need more for proper normals (different per face)
        const uint numberVertices = 24;  // 4 vertices per face * 6 faces (some faces use fewer)
        const uint numberIndices = 24;   // 8 triangles * 3 indices

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Wedge", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        if(slope_x)
        {
            // Slope along X direction
            // Vertices: bottom 4 corners, top 2 corners (at x = -hw)
            
            // Triangular face 1 (at -hd, facing -Y)
            builder.WriteFullVertex(-hw, -hd, -hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, -hd, -hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw, -hd, -hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f); // duplicate for indexing

            // Triangular face 2 (at +hd, facing +Y)
            builder.WriteFullVertex(-hw,  hd, -hh, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw,  hd,  hh, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f); // duplicate

            // Bottom face (facing -Z)
            builder.WriteFullVertex(-hw, -hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, -hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw,  hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            // Back face (at -hw, facing -X)
            builder.WriteFullVertex(-hw, -hd, -hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw,  hd, -hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(-hw,  hd,  hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);

            // Sloped face (from bottom +hw to top -hw)
            // Normal calculation: cross product of edge vectors
            float nx = 0.0f;
            float ny = 0.0f;
            float nz = width / sqrtf(width * width + height * height);
            float nzx = -height / sqrtf(width * width + height * height);
            
            builder.WriteFullVertex( hw, -hd, -hh, nzx, 0.0f, nz, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, nzx, 0.0f, nz, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex( hw,  hd, -hh, nzx, 0.0f, nz, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw,  hd,  hh, nzx, 0.0f, nz, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
        }
        else
        {
            // Slope along Y direction
            // Similar structure but rotated
            
            // Triangular face 1 (at -hw, facing -X)
            builder.WriteFullVertex(-hw, -hd, -hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw,  hd, -hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex(-hw,  hd, -hh, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);

            // Triangular face 2 (at +hw, facing +X)
            builder.WriteFullVertex( hw, -hd, -hh, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, -hd,  hh, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

            // Bottom face (facing -Z)
            builder.WriteFullVertex(-hw, -hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, -hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw,  hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw,  hd, -hh, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            // Back face (at -hd, facing -Y)
            builder.WriteFullVertex(-hw, -hd, -hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw, -hd,  hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex( hw, -hd, -hh, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

            // Sloped face
            float nx = 0.0f;
            float ny = -depth / sqrtf(depth * depth + height * height);
            float nz = depth / sqrtf(depth * depth + height * height);
            
            builder.WriteFullVertex(-hw,  hd, -hh, nx, ny, nz, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw,  hd, -hh, nx, ny, nz, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw, -hd,  hh, nx, ny, nz, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, -hd,  hh, nx, ny, nz, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Triangular face 1
            *ip++ = 0; *ip++ = 1; *ip++ = 2;

            // Triangular face 2
            *ip++ = 4; *ip++ = 5; *ip++ = 6;

            // Bottom face (2 triangles)
            *ip++ = 8; *ip++ = 9; *ip++ = 10;
            *ip++ = 8; *ip++ = 10; *ip++ = 11;

            // Back face (2 triangles)
            *ip++ = 12; *ip++ = 13; *ip++ = 14;
            *ip++ = 12; *ip++ = 14; *ip++ = 15;

            // Sloped face (2 triangles)
            *ip++ = 16; *ip++ = 17; *ip++ = 18;
            *ip++ = 17; *ip++ = 19; *ip++ = 18;
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

        return pc->CreateWithAABB(
            math::Vector3f(-hw, -hd, -hh),
            Vector3f(hw, hd, hh));
    }
} // namespace hgl::graph::inline_geometry
