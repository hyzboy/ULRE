// Arrow geometry generator for ULRE engine
// Combines a cylindrical shaft with a conical head

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateArrow(GeometryCreater *pc, const ArrowCreateInfo *aci)
    {
        if(!pc || !aci)
            return nullptr;

        const uint slices = aci->numberSlices;
        
        // Validate parameters
        if(!GeometryValidator::ValidateRevolutionParams(slices))
            return nullptr;

        const float shaft_r = aci->shaft_radius;
        const float shaft_len = aci->shaft_length;
        const float head_r = aci->head_radius;
        const float head_len = aci->head_length;

        // Shaft: cylinder side (no caps needed, head connects at top)
        // Head: cone with base
        
        // Vertex count:
        // - Shaft side: 2 rings
        // - Connection disk at shaft top: 2 rings (shaft radius and head radius)
        // - Cone side: 2 rings (base and tip)
        const uint shaft_vertices = (slices + 1) * 2;
        const uint connection_vertices = (slices + 1) * 2;
        const uint cone_side_vertices = (slices + 1) * 2;
        const uint numberVertices = shaft_vertices + connection_vertices + cone_side_vertices;

        // Index count:
        // - Shaft side: slices * 6
        // - Connection disk: slices * 6
        // - Cone side: slices * 6
        const uint numberIndices = slices * 6 + slices * 6 + slices * 6;

        // Validate counts
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Arrow", numberVertices, numberIndices))
            return nullptr;

        const float angleStep = (2.0f * math::pi) / float(slices);

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Calculate cone normal (slanted)
        const float cone_slant = sqrtf(head_r * head_r + head_len * head_len);
        const float cone_normal_z = head_r / cone_slant;
        const float cone_normal_xy = head_len / cone_slant;

        // Generate shaft cylinder (from z=0 to z=shaft_len)
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
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, 0.0f,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 0.0f);

            // Top vertex
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, shaft_len,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 1.0f);
        }

        // Connection disk at shaft_len (transition from shaft to cone base)
        // This forms a ring from shaft_r to head_r at z=shaft_len
        for(uint i = 0; i <= slices; i++)
        {
            float angle = angleStep * float(i);
            float cx = cos(angle);
            float sy = -sin(angle);

            // Normal pointing up (Z+)
            float nx = 0.0f;
            float ny = 0.0f;
            float nz = 1.0f;

            // Inner ring (shaft radius)
            builder.WriteFullVertex(cx * shaft_r, sy * shaft_r, shaft_len,
                                   nx, ny, nz,
                                   -sy, -cx, 0.0f,
                                   0.5f + cx * 0.3f, 0.5f - sy * 0.3f);

            // Outer ring (head radius)
            builder.WriteFullVertex(cx * head_r, sy * head_r, shaft_len,
                                   nx, ny, nz,
                                   -sy, -cx, 0.0f,
                                   0.5f + cx * 0.5f, 0.5f - sy * 0.5f);
        }

        // Cone head (from shaft_len to shaft_len+head_len, radius shrinks from head_r to 0)
        for(uint i = 0; i <= slices; i++)
        {
            float angle = angleStep * float(i);
            float cx = cos(angle);
            float sy = -sin(angle);

            // Slanted normal
            float nx = cx * cone_normal_xy;
            float ny = sy * cone_normal_xy;
            float nz = cone_normal_z;

            // Tangent
            float tx = -sy;
            float ty = -cx;

            // Base of cone
            builder.WriteFullVertex(cx * head_r, sy * head_r, shaft_len,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 0.0f);

            // Tip of cone
            builder.WriteFullVertex(0.0f, 0.0f, shaft_len + head_len,
                                   nx, ny, nz,
                                   tx, ty, 0.0f,
                                   float(i) / float(slices), 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            uint base = 0;

            // Shaft cylinder indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT v0 = base + i * 2;
                IndexT v1 = base + i * 2 + 1;
                IndexT v2 = base + (i + 1) * 2;
                IndexT v3 = base + (i + 1) * 2 + 1;

                *ip++ = v0;
                *ip++ = v1;
                *ip++ = v2;

                *ip++ = v2;
                *ip++ = v1;
                *ip++ = v3;
            }

            base = shaft_vertices;

            // Connection disk indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT inner0 = base + i * 2;
                IndexT outer0 = base + i * 2 + 1;
                IndexT inner1 = base + (i + 1) * 2;
                IndexT outer1 = base + (i + 1) * 2 + 1;

                *ip++ = inner0;
                *ip++ = outer0;
                *ip++ = inner1;

                *ip++ = inner1;
                *ip++ = outer0;
                *ip++ = outer1;
            }

            base = shaft_vertices + connection_vertices;

            // Cone indices
            for(uint i = 0; i < slices; i++)
            {
                IndexT v0 = base + i * 2;
                IndexT v1 = base + i * 2 + 1;
                IndexT v2 = base + (i + 1) * 2;
                IndexT v3 = base + (i + 1) * 2 + 1;

                *ip++ = v0;
                *ip++ = v1;
                *ip++ = v2;

                *ip++ = v2;
                *ip++ = v1;
                *ip++ = v3;
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
        
        bv.SetFromAABB(math::Vector3f(-max_radius, -max_radius, 0.0f),
                       Vector3f(max_radius, max_radius, total_length));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
