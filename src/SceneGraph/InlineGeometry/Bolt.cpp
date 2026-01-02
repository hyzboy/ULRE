// Bolt geometry generator for ULRE engine
// Creates a hexagonal bolt with head and shaft

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateBolt(GeometryCreater *pc, const BoltCreateInfo *bci)
    {
        if(!pc || !bci)
            return nullptr;

        const float head_r = bci->head_radius;
        const float head_h = bci->head_height;
        const float shaft_r = bci->shaft_radius;
        const float shaft_len = bci->shaft_length;
        const uint head_segs = std::max<uint>(3, bci->head_segments);

        // Validate parameters
        if(head_r <= 0.0f || head_h <= 0.0f || shaft_r <= 0.0f || shaft_len <= 0.0f)
            return nullptr;

        if(shaft_r >= head_r)
            return nullptr;

        // Simplified geometry: hexagonal head + cylindrical shaft
        const uint circ_segs = 16;  // For shaft cylinder

        const uint numberVertices = 200;  // Approximate
        const uint numberIndices = 400;   // Approximate

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Bolt", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step_head = (2.0f * std::numbers::pi_v<float>) / float(head_segs);
        const float angle_step_shaft = (2.0f * std::numbers::pi_v<float>) / float(circ_segs);

        // Head top center
        builder.WriteFullVertex(0.0f, 0.0f, head_h,
                               0.0f, 0.0f, 1.0f,
                               1.0f, 0.0f, 0.0f,
                               0.5f, 0.5f);

        // Head top vertices
        for(uint i = 0; i <= head_segs; i++)
        {
            float angle = angle_step_head * float(i);
            float x = cos(angle) * head_r;
            float y = sin(angle) * head_r;

            builder.WriteFullVertex(x, y, head_h,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f);
        }

        // Head bottom vertices (transition to shaft)
        for(uint i = 0; i <= head_segs; i++)
        {
            float angle = angle_step_head * float(i);
            float x = cos(angle) * head_r;
            float y = sin(angle) * head_r;

            float nx = cos(angle);
            float ny = sin(angle);

            builder.WriteFullVertex(x, y, 0.0f,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   0.0f, 0.0f);
        }

        // Shaft vertices
        for(uint i = 0; i <= circ_segs; i++)
        {
            float angle = angle_step_shaft * float(i);
            float x = cos(angle) * shaft_r;
            float y = sin(angle) * shaft_r;

            float nx = cos(angle);
            float ny = sin(angle);

            // Top of shaft
            builder.WriteFullVertex(x, y, 0.0f,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(circ_segs), 0.0f);

            // Bottom of shaft
            builder.WriteFullVertex(x, y, -shaft_len,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(circ_segs), 1.0f);
        }

        // Generate indices (simplified)
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Head top
            for(uint i = 0; i < head_segs; i++)
            {
                *ip++ = 0;
                *ip++ = i + 2;
                *ip++ = i + 1;
            }

            // Head sides
            IndexT head_top_base = 1;
            IndexT head_bottom_base = 1 + head_segs + 1;
            for(uint i = 0; i < head_segs; i++)
            {
                IndexT v0 = head_top_base + i;
                IndexT v1 = head_top_base + i + 1;
                IndexT v2 = head_bottom_base + i;
                IndexT v3 = head_bottom_base + i + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
            }

            // Shaft
            IndexT shaft_base = head_bottom_base + head_segs + 1;
            for(uint i = 0; i < circ_segs; i++)
            {
                IndexT v0 = shaft_base + i * 2;
                IndexT v1 = shaft_base + i * 2 + 1;
                IndexT v2 = shaft_base + (i + 1) * 2;
                IndexT v3 = shaft_base + (i + 1) * 2 + 1;

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
        bv.SetFromAABB(math::Vector3f(-head_r, -head_r, -shaft_len),
                       Vector3f(head_r, head_r, head_h));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
