// Enhanced Arrow geometry generator for ULRE engine
// Supports circular and square cross-sections with flat or concave tail

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    // Forward declarations for helper functions
    static Geometry *CreateCircularArrow(GeometryCreater *pc, const ArrowCreateInfo *aci);
    static Geometry *CreateSquareArrow(GeometryCreater *pc, const ArrowCreateInfo *aci);

    Geometry *CreateArrow(GeometryCreater *pc, const ArrowCreateInfo *aci)
    {
        if(!pc || !aci)
            return nullptr;

        if(aci->cross_section == ArrowCrossSection::Circular)
            return CreateCircularArrow(pc, aci);
        else
            return CreateSquareArrow(pc, aci);
    }

    // Circular cross-section arrow implementation
    static Geometry *CreateCircularArrow(GeometryCreater *pc, const ArrowCreateInfo *aci)
    {
        const uint slices = aci->numberSlices;
        
        // Validate parameters
        if(!GeometryValidator::ValidateRevolutionParams(slices))
            return nullptr;

        const float shaft_r = aci->shaft_radius;
        const float shaft_len = aci->shaft_length;
        const float head_r = aci->head_radius;
        const float head_len = aci->head_length;
        const bool has_concave_tail = (aci->tail_style == ArrowTailStyle::Concave);
        const float tail_depth = has_concave_tail ? aci->tail_concave_depth : 0.0f;

        // Calculate vertex and index counts
        uint numberVertices = 0;
        uint numberIndices = 0;

        // Shaft cylinder side
        numberVertices += (slices + 1) * 2;
        numberIndices += slices * 6;

        // Tail cap
        if(has_concave_tail)
        {
            // Concave tail: outer ring + center + cone ring at concave tip
            numberVertices += (slices + 1) + 1 + (slices + 1);
            numberIndices += slices * 3 + slices * 6;  // outer triangles + cone side
        }
        else
        {
            // Flat tail: center + ring
            numberVertices += 1 + (slices + 1);
            numberIndices += slices * 3;
        }

        // Connection disk at shaft top
        numberVertices += (slices + 1) * 2;
        numberIndices += slices * 6;

        // Cone head side
        numberVertices += (slices + 1) * 2;
        numberIndices += slices * 6;

        // Validate counts
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Arrow", numberVertices, numberIndices))
            return nullptr;

        const float angleStep = (2.0f * math::pi) / float(slices);

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        uint vertex_base = 0;

        // Generate shaft cylinder (from z=tail_depth to z=shaft_len)
        for(uint i = 0; i <= slices; i++)
        {
            float angle = angleStep * float(i);
            float cx = cos(angle);
            float sy = -sin(angle);

            // Normal pointing outward
            float nx = cx;
            float ny = sy;
            float nz = 0.0f;

            // Tangent
            float tx = -sy;
            float ty = -cx;

            // Bottom vertex
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, tail_depth,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 0.0f);

            // Top vertex
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, shaft_len,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 1.0f);
        }

        uint shaft_base = vertex_base;
        vertex_base += (slices + 1) * 2;

        // Generate tail cap
        uint tail_base = vertex_base;
        
        if(has_concave_tail)
        {
            // Concave tail: cone pointing inward
            // Outer ring at z=tail_depth
            for(uint i = 0; i <= slices; i++)
            {
                float angle = angleStep * float(i);
                float cx = cos(angle);
                float sy = -sin(angle);

                builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, tail_depth,
                                       0.0f, 0.0f, -1.0f,
                                       -sy, -cx, 0.0f,
                                       0.5f + cx * 0.5f, 0.5f - sy * 0.5f);
            }

            // Center point at z=0
            builder.WriteFullVertex(0.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, -1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.5f, 0.5f);

            uint center_index = vertex_base + slices + 1;

            // Concave cone side (slanted inward)
            float cone_slant = sqrtf(shaft_r * shaft_r + tail_depth * tail_depth);
            float cone_nz = -shaft_r / cone_slant;
            float cone_nxy = tail_depth / cone_slant;

            for(uint i = 0; i <= slices; i++)
            {
                float angle = angleStep * float(i);
                float cx = cos(angle);
                float sy = -sin(angle);

                builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, tail_depth,
                                       cx * cone_nxy, sy * cone_nxy, cone_nz,
                                       -sy, -cx, 0.0f,
                                       float(i) / float(slices), 1.0f);
            }

            vertex_base += (slices + 1) + 1 + (slices + 1);
        }
        else
        {
            // Flat tail: simple disk
            // Center point
            builder.WriteFullVertex(0.0f, 0.0f, tail_depth,
                                   0.0f, 0.0f, -1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.5f, 0.5f);

            // Outer ring
            for(uint i = 0; i <= slices; i++)
            {
                float angle = angleStep * float(i);
                float cx = cos(angle);
                float sy = -sin(angle);

                builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, tail_depth,
                                       0.0f, 0.0f, -1.0f,
                                       -sy, -cx, 0.0f,
                                       0.5f + cx * 0.5f, 0.5f - sy * 0.5f);
            }

            vertex_base += 1 + (slices + 1);
        }

        // Connection disk at shaft_len
        uint connection_base = vertex_base;
        for(uint i = 0; i <= slices; i++)
        {
            float angle = angleStep * float(i);
            float cx = cos(angle);
            float sy = -sin(angle);

            // Inner ring (shaft radius)
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, shaft_len,
                                   0.0f, 0.0f, 1.0f,
                                   -sy, -cx, 0.0f,
                                   0.5f + cx * 0.3f, 0.5f - sy * 0.3f);

            // Outer ring (head radius)
            builder.WriteFullVertex(cx * head_r, sy * head_r, shaft_len,
                                   0.0f, 0.0f, 1.0f,
                                   -sy, -cx, 0.0f,
                                   0.5f + cx * 0.5f, 0.5f - sy * 0.5f);
        }

        vertex_base += (slices + 1) * 2;

        // Cone head
        float cone_slant = sqrtf(head_r * head_r + head_len * head_len);
        float cone_normal_z = head_r / cone_slant;
        float cone_normal_xy = head_len / cone_slant;

        uint cone_base = vertex_base;
        for(uint i = 0; i <= slices; i++)
        {
            float angle = angleStep * float(i);
            float cx = cos(angle);
            float sy = -sin(angle);

            float nx = cx * cone_normal_xy;
            float ny = sy * cone_normal_xy;
            float nz = cone_normal_z;

            // Base of cone
            builder.WriteFullVertex(cx * head_r, sy * head_r, shaft_len,
                                   nx, ny, nz,
                                   -sy, -cx, 0.0f,
                                   float(i) / float(slices), 0.0f);

            // Tip of cone
            builder.WriteFullVertex(0.0f, 0.0f, shaft_len + head_len,
                                   nx, ny, nz,
                                   -sy, -cx, 0.0f,
                                   float(i) / float(slices), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Shaft cylinder indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT v0 = shaft_base + i * 2;
                IndexT v1 = shaft_base + i * 2 + 1;
                IndexT v2 = shaft_base + (i + 1) * 2;
                IndexT v3 = shaft_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
            }

            // Tail cap indices
            if(has_concave_tail)
            {
                uint outer_ring = tail_base;
                uint center = tail_base + slices + 1;
                uint cone_ring = tail_base + slices + 2;

                // Top disk triangles
                for(uint i = 0; i < slices; i++)
                {
                    *ip++ = outer_ring + i;
                    *ip++ = center;
                    *ip++ = outer_ring + i + 1;
                }

                // Concave cone side
                for(uint i = 0; i < slices; i++)
                {
                    IndexT v0 = cone_ring + i;
                    IndexT v1 = center;
                    IndexT v2 = cone_ring + i + 1;
                    IndexT v3 = center;

                    *ip++ = v0; *ip++ = v1; *ip++ = v2;
                    *ip++ = v2; *ip++ = v1; *ip++ = v3;
                }
            }
            else
            {
                // Flat tail triangles
                uint center = tail_base;
                uint ring = tail_base + 1;

                for(uint i = 0; i < slices; i++)
                {
                    *ip++ = center;
                    *ip++ = ring + i + 1;
                    *ip++ = ring + i;
                }
            }

            // Connection disk indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT inner0 = connection_base + i * 2;
                IndexT outer0 = connection_base + i * 2 + 1;
                IndexT inner1 = connection_base + (i + 1) * 2;
                IndexT outer1 = connection_base + (i + 1) * 2 + 1;

                *ip++ = inner0; *ip++ = outer0; *ip++ = inner1;
                *ip++ = inner1; *ip++ = outer0; *ip++ = outer1;
            }

            // Cone indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT v0 = cone_base + i * 2;
                IndexT v1 = cone_base + i * 2 + 1;
                IndexT v2 = cone_base + (i + 1) * 2;
                IndexT v3 = cone_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
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
        float max_radius = std::max(shaft_r, head_r);
        float total_length = shaft_len + head_len;
        
        bv.SetFromAABB(Vector3f(-max_radius, -max_radius, 0.0f),
                       Vector3f(max_radius, max_radius, total_length));

        p->SetBoundingVolumes(bv);

        return p;
    }

    // Square cross-section arrow implementation
    static Geometry *CreateSquareArrow(GeometryCreater *pc, const ArrowCreateInfo *aci)
    {
        const float shaft_hw = aci->shaft_radius;  // half-width
        const float shaft_len = aci->shaft_length;
        const float head_hw = aci->head_radius;  // half-width
        const float head_len = aci->head_length;
        const bool has_concave_tail = (aci->tail_style == ArrowTailStyle::Concave);
        const float tail_depth = has_concave_tail ? aci->tail_concave_depth : 0.0f;

        // Calculate vertex and index counts
        // Square shaft: 4 sides, each with 2 triangles
        uint numberVertices = 0;
        uint numberIndices = 0;

        // Shaft: 4 faces, each 4 vertices
        numberVertices += 4 * 4;
        numberIndices += 4 * 6;

        // Tail cap
        if(has_concave_tail)
        {
            // Outer square + center + inner pyramid (4 triangular faces)
            numberVertices += 4 + 1 + 4 * 3;
            numberIndices += 4 * 3 + 4 * 3;
        }
        else
        {
            // Flat tail: 4 corners + center
            numberVertices += 4 + 1;
            numberIndices += 4 * 3;
        }

        // Connection square (transition from shaft to head)
        numberVertices += 8;  // 4 inner + 4 outer
        numberIndices += 4 * 6;

        // Pyramid head: 4 triangular faces
        numberVertices += 4 * 3;
        numberIndices += 4 * 3;

        // Validate counts
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Arrow", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        uint vertex_base = 0;

        // Generate shaft (4 sides: +X, -X, +Y, -Y)
        uint shaft_base = vertex_base;
        
        // Define square corners
        float corners[4][2] = {
            {shaft_hw, shaft_hw},    // 0: +X, +Y
            {-shaft_hw, shaft_hw},   // 1: -X, +Y
            {-shaft_hw, -shaft_hw},  // 2: -X, -Y
            {shaft_hw, -shaft_hw}    // 3: +X, -Y
        };

        // Generate 4 side faces
        for(int side = 0; side < 4; side++)
        {
            int c0 = side;
            int c1 = (side + 1) % 4;

            float x0 = corners[c0][0], y0 = corners[c0][1];
            float x1 = corners[c1][0], y1 = corners[c1][1];

            // Normal perpendicular to face
            float dx = x1 - x0, dy = y1 - y0;
            float nx = dy, ny = -dx;
            float len = sqrtf(nx * nx + ny * ny);
            nx /= len; ny /= len;

            // Tangent along edge
            float tx = dx / len, ty = dy / len;

            // Bottom edge
            builder.WriteFullVertex(x0, y0, tail_depth, nx, ny, 0.0f, tx, ty, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(x1, y1, tail_depth, nx, ny, 0.0f, tx, ty, 0.0f, 1.0f, 0.0f);

            // Top edge
            builder.WriteFullVertex(x0, y0, shaft_len, nx, ny, 0.0f, tx, ty, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex(x1, y1, shaft_len, nx, ny, 0.0f, tx, ty, 0.0f, 1.0f, 1.0f);
        }

        vertex_base += 4 * 4;

        // Generate tail cap
        uint tail_base = vertex_base;
        
        if(has_concave_tail)
        {
            // Outer square at z=tail_depth
            for(int i = 0; i < 4; i++)
            {
                builder.WriteFullVertex(corners[i][0], corners[i][1], tail_depth,
                                       0.0f, 0.0f, -1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       corners[i][0] / shaft_hw * 0.5f + 0.5f,
                                       corners[i][1] / shaft_hw * 0.5f + 0.5f);
            }

            // Center point at z=0
            builder.WriteFullVertex(0.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f, -1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.5f, 0.5f);

            uint center_idx = vertex_base + 4;

            // 4 pyramid faces (slanted inward)
            for(int side = 0; side < 4; side++)
            {
                int c0 = side;
                int c1 = (side + 1) % 4;

                float x0 = corners[c0][0], y0 = corners[c0][1];
                float x1 = corners[c1][0], y1 = corners[c1][1];

                // Calculate normal for inward-slanted face
                Vector3f v0(x0, y0, tail_depth);
                Vector3f v1(x1, y1, tail_depth);
                Vector3f v2(0.0f, 0.0f, 0.0f);

                Vector3f edge1 = v1 - v0;
                Vector3f edge2 = v2 - v0;
                
                // Calculate cross product manually
                float normal_x = edge1.y * edge2.z - edge1.z * edge2.y;
                float normal_y = edge1.z * edge2.x - edge1.x * edge2.z;
                float normal_z = edge1.x * edge2.y - edge1.y * edge2.x;
                
                // Normalize
                float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
                if(normal_len > 0.0001f)
                {
                    normal_x /= normal_len;
                    normal_y /= normal_len;
                    normal_z /= normal_len;
                }

                builder.WriteFullVertex(x0, y0, tail_depth, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
                builder.WriteFullVertex(x1, y1, tail_depth, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
                builder.WriteFullVertex(0.0f, 0.0f, 0.0f, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f);
            }

            vertex_base += 4 + 1 + 4 * 3;
        }
        else
        {
            // Center point
            builder.WriteFullVertex(0.0f, 0.0f, tail_depth,
                                   0.0f, 0.0f, -1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.5f, 0.5f);

            // 4 corners
            for(int i = 0; i < 4; i++)
            {
                builder.WriteFullVertex(corners[i][0], corners[i][1], tail_depth,
                                       0.0f, 0.0f, -1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       corners[i][0] / shaft_hw * 0.5f + 0.5f,
                                       corners[i][1] / shaft_hw * 0.5f + 0.5f);
            }

            vertex_base += 5;
        }

        // Connection square at shaft_len
        uint connection_base = vertex_base;
        
        float head_corners[4][2] = {
            {head_hw, head_hw},
            {-head_hw, head_hw},
            {-head_hw, -head_hw},
            {head_hw, -head_hw}
        };

        // Inner square (shaft size) pointing up
        for(int i = 0; i < 4; i++)
        {
            builder.WriteFullVertex(corners[i][0], corners[i][1], shaft_len,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   corners[i][0] / shaft_hw * 0.3f + 0.5f,
                                   corners[i][1] / shaft_hw * 0.3f + 0.5f);
        }

        // Outer square (head size) pointing up
        for(int i = 0; i < 4; i++)
        {
            builder.WriteFullVertex(head_corners[i][0], head_corners[i][1], shaft_len,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   head_corners[i][0] / head_hw * 0.5f + 0.5f,
                                   head_corners[i][1] / head_hw * 0.5f + 0.5f);
        }

        vertex_base += 8;

        // Pyramid head (4 triangular faces)
        uint head_base = vertex_base;
        
        for(int side = 0; side < 4; side++)
        {
            int c0 = side;
            int c1 = (side + 1) % 4;

            float x0 = head_corners[c0][0], y0 = head_corners[c0][1];
            float x1 = head_corners[c1][0], y1 = head_corners[c1][1];

            // Calculate normal for outward-slanted face
            Vector3f v0(x0, y0, shaft_len);
            Vector3f v1(x1, y1, shaft_len);
            Vector3f v2(0.0f, 0.0f, shaft_len + head_len);

            Vector3f edge1 = v1 - v0;
            Vector3f edge2 = v2 - v0;
            
            // Calculate cross product manually
            float normal_x = edge1.y * edge2.z - edge1.z * edge2.y;
            float normal_y = edge1.z * edge2.x - edge1.x * edge2.z;
            float normal_z = edge1.x * edge2.y - edge1.y * edge2.x;
            
            // Normalize
            float normal_len = sqrtf(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
            if(normal_len > 0.0001f)
            {
                normal_x /= normal_len;
                normal_y /= normal_len;
                normal_z /= normal_len;
            }

            builder.WriteFullVertex(x0, y0, shaft_len, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(x1, y1, shaft_len, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(0.0f, 0.0f, shaft_len + head_len, normal_x, normal_y, normal_z, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f);
        }

        vertex_base += 4 * 3;

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Shaft faces
            for(uint side = 0; side < 4; side++)
            {
                IndexT base = shaft_base + side * 4;
                *ip++ = base + 0; *ip++ = base + 1; *ip++ = base + 2;
                *ip++ = base + 2; *ip++ = base + 1; *ip++ = base + 3;
            }

            // Tail cap
            if(has_concave_tail)
            {
                // Top square triangles
                for(uint i = 0; i < 4; i++)
                {
                    *ip++ = tail_base + 4;  // center
                    *ip++ = tail_base + i;
                    *ip++ = tail_base + (i + 1) % 4;
                }

                // Pyramid faces
                for(uint i = 0; i < 4; i++)
                {
                    IndexT base = tail_base + 5 + i * 3;
                    *ip++ = base + 0;
                    *ip++ = base + 1;
                    *ip++ = base + 2;
                }
            }
            else
            {
                // Flat tail triangles
                for(uint i = 0; i < 4; i++)
                {
                    *ip++ = tail_base;  // center
                    *ip++ = tail_base + 1 + (i + 1) % 4;
                    *ip++ = tail_base + 1 + i;
                }
            }

            // Connection square (4 trapezoidal faces)
            for(uint i = 0; i < 4; i++)
            {
                IndexT inner0 = connection_base + i;
                IndexT inner1 = connection_base + (i + 1) % 4;
                IndexT outer0 = connection_base + 4 + i;
                IndexT outer1 = connection_base + 4 + (i + 1) % 4;

                *ip++ = inner0; *ip++ = outer0; *ip++ = inner1;
                *ip++ = inner1; *ip++ = outer0; *ip++ = outer1;
            }

            // Pyramid head faces
            for(uint i = 0; i < 4; i++)
            {
                IndexT base = head_base + i * 3;
                *ip++ = base + 0;
                *ip++ = base + 1;
                *ip++ = base + 2;
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
        float max_hw = std::max(shaft_hw, head_hw);
        float total_length = shaft_len + head_len;
        
        bv.SetFromAABB(Vector3f(-max_hw, -max_hw, 0.0f),
                       Vector3f(max_hw, max_hw, total_length));

        p->SetBoundingVolumes(bv);

        return p;
    }

} // namespace hgl::graph::inline_geometry
