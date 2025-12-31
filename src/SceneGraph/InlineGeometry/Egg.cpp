// Egg/ovoid geometry generator for ULRE engine
// Creates a 3D egg shape with asymmetric ellipsoid

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateEgg(GeometryCreater *pc, const EggCreateInfo *eci)
    {
        if(!pc || !eci)
            return nullptr;

        const float radius = eci->radius;
        const float length_ratio = eci->length_ratio;
        const float bottom_scale = eci->bottom_scale;
        const uint slices = std::max<uint>(8, eci->slices);
        const uint stacks = std::max<uint>(4, eci->stacks);

        // Validate parameters
        if(radius <= 0.0f || length_ratio <= 0.0f || bottom_scale <= 0.0f)
            return nullptr;

        const float total_height = radius * length_ratio * 2.0f;

        // Vertices: (stacks + 1) * (slices + 1)
        const uint numberVertices = (stacks + 1) * (slices + 1);

        // Indices: stacks * slices * 6
        const uint numberIndices = stacks * slices * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Egg", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * math::pi) / float(slices);

        // Generate vertices using modified sphere equations
        for(uint i = 0; i <= stacks; i++)
        {
            float v = float(i) / float(stacks);
            // Stack angle from -pi/2 (bottom) to +pi/2 (top)
            float stack_angle = -math::pi * 0.5f + math::pi * v;
            
            float cos_stack = cos(stack_angle);
            float sin_stack = sin(stack_angle);

            // Modify radius based on vertical position to create egg shape
            float r_scale;
            if(sin_stack < 0.0f)
            {
                // Bottom half: scale down
                r_scale = radius * (1.0f + sin_stack * (1.0f - bottom_scale));
            }
            else
            {
                // Top half: keep circular
                r_scale = radius;
            }

            float z = sin_stack * total_height * 0.5f;
            float ring_radius = cos_stack * r_scale;

            for(uint j = 0; j <= slices; j++)
            {
                float u = float(j) / float(slices);
                float slice_angle = angle_step * float(j);

                float x = cos(slice_angle) * ring_radius;
                float y = sin(slice_angle) * ring_radius;

                // Normal calculation for egg shape (approximate)
                float nx = cos(slice_angle) * cos_stack;
                float ny = sin(slice_angle) * cos_stack;
                float nz = sin_stack;

                // Normalize
                float n_len = sqrtf(nx * nx + ny * ny + nz * nz);
                if(n_len > 0.0f)
                {
                    nx /= n_len;
                    ny /= n_len;
                    nz /= n_len;
                }

                // Tangent
                float tx = -sin(slice_angle);
                float ty = cos(slice_angle);
                float tz = 0.0f;

                builder.WriteFullVertex(x, y, z,
                                       nx, ny, nz,
                                       tx, ty, tz,
                                       u, v);
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            for(uint i = 0; i < stacks; i++)
            {
                for(uint j = 0; j < slices; j++)
                {
                    IndexT v0 = i * (slices + 1) + j;
                    IndexT v1 = i * (slices + 1) + j + 1;
                    IndexT v2 = (i + 1) * (slices + 1) + j;
                    IndexT v3 = (i + 1) * (slices + 1) + j + 1;

                    *ip++ = v0;
                    *ip++ = v2;
                    *ip++ = v1;

                    *ip++ = v1;
                    *ip++ = v2;
                    *ip++ = v3;
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
        float max_r = radius * std::max(1.0f, bottom_scale);
        BoundingVolumes bv;
        bv.SetFromAABB(math::Vector3f(-max_r, -max_r, -total_height * 0.5f),
                       Vector3f(max_r, max_r, total_height * 0.5f));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
