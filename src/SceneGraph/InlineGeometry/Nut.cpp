// Nut geometry generator for ULRE engine
// Creates a hexagonal nut with threaded hole

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateNut(GeometryCreater *pc, const NutCreateInfo *nci)
    {
        if(!pc || !nci)
            return nullptr;

        const float outer_r = nci->outer_radius;
        const float inner_r = nci->inner_radius;
        const float height = nci->height;
        const uint sides = std::max<uint>(3, nci->sides);

        // Validate parameters
        if(outer_r <= 0.0f || inner_r <= 0.0f || height <= 0.0f)
            return nullptr;

        if(inner_r >= outer_r)
            return nullptr;

        const float half_h = height * 0.5f;

        // Nut is a hexagonal prism with circular hole
        const uint circ_segs = 16;  // For inner hole

        const uint numberVertices = 200;  // Approximate
        const uint numberIndices = 400;   // Approximate

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Nut", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step_outer = (2.0f * std::numbers::pi_v<float>) / float(sides);
        const float angle_step_inner = (2.0f * std::numbers::pi_v<float>) / float(circ_segs);

        // Top face - outer ring
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_step_outer * float(i);
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            builder.WriteFullVertex(x, y, half_h,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f);
        }

        // Top face - inner ring
        for(uint i = 0; i <= circ_segs; i++)
        {
            float angle = angle_step_inner * float(i);
            float x = cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            builder.WriteFullVertex(x, y, half_h,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f);
        }

        // Bottom face - outer ring
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_step_outer * float(i);
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            builder.WriteFullVertex(x, y, -half_h,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f);
        }

        // Bottom face - inner ring
        for(uint i = 0; i <= circ_segs; i++)
        {
            float angle = angle_step_inner * float(i);
            float x = cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            builder.WriteFullVertex(x, y, -half_h,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   0.0f, 0.0f);
        }

        // Outer sides
        for(uint i = 0; i <= sides; i++)
        {
            float angle = angle_step_outer * float(i);
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            float nx = cos(angle);
            float ny = sin(angle);

            builder.WriteFullVertex(x, y, -half_h,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(sides), 0.0f);

            builder.WriteFullVertex(x, y, half_h,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(sides), 1.0f);
        }

        // Inner hole (inverted normals)
        for(uint i = 0; i <= circ_segs; i++)
        {
            float angle = angle_step_inner * float(i);
            float x = cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            float nx = -cos(angle);
            float ny = -sin(angle);

            builder.WriteFullVertex(x, y, -half_h,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(circ_segs), 0.0f);

            builder.WriteFullVertex(x, y, half_h,
                                   nx, ny, 0.0f,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(circ_segs), 1.0f);
        }

        // Generate indices (simplified)
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Top face
            for(uint i = 0; i < sides + circ_segs; i++)
            {
                *ip++ = 0;
                *ip++ = i + 1;
                *ip++ = i;
            }

            // Bottom face
            IndexT bottom_base = (sides + 1) + (circ_segs + 1);
            for(uint i = 0; i < sides + circ_segs; i++)
            {
                *ip++ = bottom_base;
                *ip++ = bottom_base + i;
                *ip++ = bottom_base + i + 1;
            }

            // Outer sides
            IndexT outer_side_base = bottom_base + (sides + 1) + (circ_segs + 1);
            for(uint i = 0; i < sides; i++)
            {
                IndexT v0 = outer_side_base + i * 2;
                IndexT v1 = outer_side_base + i * 2 + 1;
                IndexT v2 = outer_side_base + (i + 1) * 2;
                IndexT v3 = outer_side_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
            }

            // Inner hole
            IndexT inner_side_base = outer_side_base + (sides + 1) * 2;
            for(uint i = 0; i < circ_segs; i++)
            {
                IndexT v0 = inner_side_base + i * 2;
                IndexT v1 = inner_side_base + i * 2 + 1;
                IndexT v2 = inner_side_base + (i + 1) * 2;
                IndexT v3 = inner_side_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v2; *ip++ = v1;
                *ip++ = v2; *ip++ = v3; *ip++ = v1;
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
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, -half_h),
                       Vector3f(outer_r, outer_r, half_h));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
