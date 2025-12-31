// Spiral stairs geometry generator for ULRE engine
// Creates a spiral staircase wrapping around a central axis

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateSpiralStairs(GeometryCreater *pc, const SpiralStairsCreateInfo *ssci)
    {
        if(!pc || !ssci)
            return nullptr;

        const float inner_r = ssci->inner_radius;
        const float outer_r = ssci->outer_radius;
        const float step_h = ssci->step_height;
        const float total_angle = ssci->total_angle;
        const uint step_count = std::max<uint>(1, ssci->step_count);
        const bool clockwise = ssci->clockwise;

        // Validate parameters
        if(inner_r >= outer_r || step_h <= 0.0f || total_angle <= 0.0f)
            return nullptr;

        const float angle_per_step = deg2rad(total_angle / float(step_count));
        const float total_height = step_h * step_count;

        // Each step is a sector of an annulus (ring segment)
        // Vertices: 4 corners * multiple faces = simplified to quads
        const uint verts_per_step = 24;  // 6 faces with 4 vertices each (some shared conceptually)
        const uint numberVertices = verts_per_step * step_count;

        const uint indices_per_step = 36;  // 6 faces * 6 indices
        const uint numberIndices = indices_per_step * step_count;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("SpiralStairs", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        float direction = clockwise ? -1.0f : 1.0f;

        // Generate each step
        for(uint i = 0; i < step_count; i++)
        {
            float z_bottom = i * step_h;
            float z_top = (i + 1) * step_h;
            
            float angle_start = direction * i * angle_per_step;
            float angle_end = direction * (i + 1) * angle_per_step;

            // Calculate positions
            float cos_start = cos(angle_start);
            float sin_start = sin(angle_start);
            float cos_end = cos(angle_end);
            float sin_end = sin(angle_end);

            // Inner edge positions
            float ix0 = cos_start * inner_r;
            float iy0 = sin_start * inner_r;
            float ix1 = cos_end * inner_r;
            float iy1 = sin_end * inner_r;

            // Outer edge positions
            float ox0 = cos_start * outer_r;
            float oy0 = sin_start * outer_r;
            float ox1 = cos_end * outer_r;
            float oy1 = sin_end * outer_r;

            // Top face
            builder.WriteFullVertex(ix0, iy0, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ox0, oy0, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(ox1, oy1, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ix1, iy1, z_top, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            // Bottom face
            builder.WriteFullVertex(ix0, iy0, z_bottom, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ix1, iy1, z_bottom, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            builder.WriteFullVertex(ox1, oy1, z_bottom, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ox0, oy0, z_bottom, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

            // Inner face (vertical)
            float inx = -cos_start * 0.5f - cos_end * 0.5f;
            float iny = -sin_start * 0.5f - sin_end * 0.5f;
            float in_len = sqrtf(inx * inx + iny * iny);
            if(in_len > 0.0f) { inx /= in_len; iny /= in_len; }

            builder.WriteFullVertex(ix0, iy0, z_bottom, inx, iny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ix0, iy0, z_top, inx, iny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
            builder.WriteFullVertex(ix1, iy1, z_top, inx, iny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ix1, iy1, z_bottom, inx, iny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

            // Outer face (vertical)
            float onx = cos_start * 0.5f + cos_end * 0.5f;
            float ony = sin_start * 0.5f + sin_end * 0.5f;
            float on_len = sqrtf(onx * onx + ony * ony);
            if(on_len > 0.0f) { onx /= on_len; ony /= on_len; }

            builder.WriteFullVertex(ox0, oy0, z_bottom, onx, ony, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ox1, oy1, z_bottom, onx, ony, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(ox1, oy1, z_top, onx, ony, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ox0, oy0, z_top, onx, ony, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

            // Start radial face
            float snx = -sin_start * direction;
            float sny = cos_start * direction;

            builder.WriteFullVertex(ix0, iy0, z_bottom, snx, sny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ox0, oy0, z_bottom, snx, sny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
            builder.WriteFullVertex(ox0, oy0, z_top, snx, sny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ix0, iy0, z_top, snx, sny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);

            // End radial face
            float enx = sin_end * direction;
            float eny = -cos_end * direction;

            builder.WriteFullVertex(ix1, iy1, z_bottom, enx, eny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            builder.WriteFullVertex(ix1, iy1, z_top, enx, eny, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
            builder.WriteFullVertex(ox1, oy1, z_top, enx, eny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
            builder.WriteFullVertex(ox1, oy1, z_bottom, enx, eny, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            for(uint i = 0; i < step_count; i++)
            {
                IndexT base = i * 24;

                // Each step has 6 faces
                for(uint f = 0; f < 6; f++)
                {
                    IndexT v0 = base + f * 4 + 0;
                    IndexT v1 = base + f * 4 + 1;
                    IndexT v2 = base + f * 4 + 2;
                    IndexT v3 = base + f * 4 + 3;

                    *ip++ = v0; *ip++ = v1; *ip++ = v2;
                    *ip++ = v0; *ip++ = v2; *ip++ = v3;
                }
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
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, 0.0f),
                       Vector3f(outer_r, outer_r, total_height));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
