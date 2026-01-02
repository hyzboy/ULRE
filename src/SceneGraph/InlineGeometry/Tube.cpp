// Tube geometry generator for ULRE engine
// Creates a straight pipe/tube with thickness

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateTube(GeometryCreater *pc, const TubeCreateInfo *tci)
    {
        if(!pc || !tci)
            return nullptr;

        const float length = tci->length;
        const float outer_r = tci->outer_radius;
        const float inner_r = tci->inner_radius;
        const uint segments = std::max<uint>(8, tci->segments);
        const bool gen_caps = tci->generate_caps;

        // Validate parameters
        if(length <= 0.0f || outer_r <= 0.0f || inner_r <= 0.0f)
            return nullptr;

        if(inner_r >= outer_r)
            return nullptr;

        const float half_len = length * 0.5f;

        // Vertices: outer cylinder + inner cylinder + optional caps
        const uint ring_verts = segments + 1;
        const uint outer_verts = ring_verts * 2;  // Top and bottom
        const uint inner_verts = ring_verts * 2;
        const uint cap_verts = gen_caps ? ring_verts * 4 : 0;  // 2 caps * 2 rings each

        const uint numberVertices = outer_verts + inner_verts + cap_verts;

        const uint outer_indices = segments * 6;
        const uint inner_indices = segments * 6;
        const uint cap_indices = gen_caps ? segments * 12 : 0;

        const uint numberIndices = outer_indices + inner_indices + cap_indices;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Tube", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(segments);

        // Outer surface
        for(uint i = 0; i <= segments; i++)
        {
            float angle = angle_step * float(i);
            float x = cos(angle) * outer_r;
            float y = sin(angle) * outer_r;

            float nx = cos(angle);
            float ny = sin(angle);
            float nz = 0.0f;

            // Bottom
            builder.WriteFullVertex(x, y, -half_len,
                                   nx, ny, nz,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(segments), 0.0f);

            // Top
            builder.WriteFullVertex(x, y, half_len,
                                   nx, ny, nz,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(segments), 1.0f);
        }

        // Inner surface (inverted normals)
        for(uint i = 0; i <= segments; i++)
        {
            float angle = angle_step * float(i);
            float x = cos(angle) * inner_r;
            float y = sin(angle) * inner_r;

            float nx = -cos(angle);
            float ny = -sin(angle);
            float nz = 0.0f;

            // Bottom
            builder.WriteFullVertex(x, y, -half_len,
                                   nx, ny, nz,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(segments), 0.0f);

            // Top
            builder.WriteFullVertex(x, y, half_len,
                                   nx, ny, nz,
                                   -sin(angle), cos(angle), 0.0f,
                                   float(i) / float(segments), 1.0f);
        }

        // Caps (if enabled)
        if(gen_caps)
        {
            // Bottom cap
            for(uint i = 0; i <= segments; i++)
            {
                float angle = angle_step * float(i);
                float x_outer = cos(angle) * outer_r;
                float y_outer = sin(angle) * outer_r;
                float x_inner = cos(angle) * inner_r;
                float y_inner = sin(angle) * inner_r;

                builder.WriteFullVertex(x_outer, y_outer, -half_len,
                                       0.0f, 0.0f, -1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f);

                builder.WriteFullVertex(x_inner, y_inner, -half_len,
                                       0.0f, 0.0f, -1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f);
            }

            // Top cap
            for(uint i = 0; i <= segments; i++)
            {
                float angle = angle_step * float(i);
                float x_outer = cos(angle) * outer_r;
                float y_outer = sin(angle) * outer_r;
                float x_inner = cos(angle) * inner_r;
                float y_inner = sin(angle) * inner_r;

                builder.WriteFullVertex(x_outer, y_outer, half_len,
                                       0.0f, 0.0f, 1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f);

                builder.WriteFullVertex(x_inner, y_inner, half_len,
                                       0.0f, 0.0f, 1.0f,
                                       1.0f, 0.0f, 0.0f,
                                       0.0f, 0.0f);
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Outer surface
            for(uint i = 0; i < segments; i++)
            {
                IndexT v0 = i * 2;
                IndexT v1 = i * 2 + 1;
                IndexT v2 = (i + 1) * 2;
                IndexT v3 = (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v1; *ip++ = v2;
                *ip++ = v2; *ip++ = v1; *ip++ = v3;
            }

            // Inner surface
            IndexT inner_base = outer_verts;
            for(uint i = 0; i < segments; i++)
            {
                IndexT v0 = inner_base + i * 2;
                IndexT v1 = inner_base + i * 2 + 1;
                IndexT v2 = inner_base + (i + 1) * 2;
                IndexT v3 = inner_base + (i + 1) * 2 + 1;

                *ip++ = v0; *ip++ = v2; *ip++ = v1;
                *ip++ = v2; *ip++ = v3; *ip++ = v1;
            }

            // Caps
            if(gen_caps)
            {
                IndexT cap_base = outer_verts + inner_verts;

                // Bottom cap
                for(uint i = 0; i < segments; i++)
                {
                    IndexT o0 = cap_base + i * 2;
                    IndexT i0 = cap_base + i * 2 + 1;
                    IndexT o1 = cap_base + (i + 1) * 2;
                    IndexT i1 = cap_base + (i + 1) * 2 + 1;

                    *ip++ = o0; *ip++ = i0; *ip++ = o1;
                    *ip++ = i0; *ip++ = i1; *ip++ = o1;
                }

                // Top cap
                IndexT top_base = cap_base + ring_verts * 2;
                for(uint i = 0; i < segments; i++)
                {
                    IndexT o0 = top_base + i * 2;
                    IndexT i0 = top_base + i * 2 + 1;
                    IndexT o1 = top_base + (i + 1) * 2;
                    IndexT i1 = top_base + (i + 1) * 2 + 1;

                    *ip++ = o0; *ip++ = o1; *ip++ = i0;
                    *ip++ = i0; *ip++ = o1; *ip++ = i1;
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
        bv.SetFromAABB(math::Vector3f(-outer_r, -outer_r, -half_len),
                       Vector3f(outer_r, outer_r, half_len));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
