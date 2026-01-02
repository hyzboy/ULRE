// Straight stairs geometry generator for ULRE engine
// Creates a straight staircase with steps and optional side panels

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateStraightStairs(GeometryCreater *pc, const StraightStairsCreateInfo *ssci)
    {
        if(!pc || !ssci)
            return nullptr;

        const float step_width = ssci->step_width;
        const float step_depth = ssci->step_depth;
        const float step_height = ssci->step_height;
        const uint step_count = std::max<uint>(1, ssci->step_count);
        const float side_thick = ssci->side_thickness;
        const bool gen_sides = ssci->generate_sides;

        // Validate parameters
        if(step_width <= 0.0f || step_depth <= 0.0f || step_height <= 0.0f)
            return nullptr;

        const float hw = step_width * 0.5f;
        const float total_length = step_depth * step_count;
        const float total_height = step_height * step_count;

        // Each step: top face + riser face = 12 vertices (6 per quad)
        // Sides (if enabled): 2 side panels
        uint verts_per_step = 24;  // 4 quads per step
        uint verts_sides = gen_sides ? step_count * 24 : 0;  // Side panels
        const uint numberVertices = verts_per_step * step_count + verts_sides;

        uint indices_per_step = 12;  // 2 quads * 6 indices
        uint indices_sides = gen_sides ? step_count * 12 : 0;
        const uint numberIndices = indices_per_step * step_count + indices_sides;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("StraightStairs", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Generate steps
        for(uint i = 0; i < step_count; i++)
        {
            float z_base = i * step_height;
            float z_top = (i + 1) * step_height;
            float y_front = i * step_depth;
            float y_back = (i + 1) * step_depth;

            // Top face of step (horizontal surface)
            builder.WriteFullVertex(-hw, y_back, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, y_back, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw, y_front, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, y_front, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            // Riser face (vertical surface)
            builder.WriteFullVertex(-hw, y_front, z_base, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, y_front, z_base, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw, y_front, z_top, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, y_front, z_top, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            // Left edge
            builder.WriteFullVertex(-hw, y_back, z_top, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw, y_front, z_top, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(-hw, y_front, z_base, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, y_back, z_base, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);

            // Right edge
            builder.WriteFullVertex( hw, y_back, z_top, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, y_back, z_base, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw, y_front, z_base, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex( hw, y_front, z_top, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

            // Back edge
            builder.WriteFullVertex(-hw, y_back, z_top, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(-hw, y_back, z_base, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex( hw, y_back, z_base, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex( hw, y_back, z_top, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

            // Bottom face
            builder.WriteFullVertex(-hw, y_back, z_base, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex( hw, y_back, z_base, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex( hw, y_front, z_base, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(-hw, y_front, z_base, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            for(uint i = 0; i < step_count; i++)
            {
                IndexT base = i * 24;

                // Top face
                *ip++ = base + 0; *ip++ = base + 1; *ip++ = base + 2;
                *ip++ = base + 0; *ip++ = base + 2; *ip++ = base + 3;

                // Riser
                *ip++ = base + 4; *ip++ = base + 5; *ip++ = base + 6;
                *ip++ = base + 4; *ip++ = base + 6; *ip++ = base + 7;
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

        return pc->CreateWithAABB(
            math::Vector3f(-hw, 0.0f, 0.0f),
            Vector3f(hw, total_length, total_height));
    }
} // namespace hgl::graph::inline_geometry
