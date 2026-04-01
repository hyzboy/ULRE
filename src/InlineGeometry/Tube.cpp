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
        const uint stacks = std::max<uint>(1, tci->stacks);
        const uint vertical_levels = stacks + 1; // number of z-levels per ring

        const uint outer_verts = ring_verts * vertical_levels;
        const uint inner_verts = ring_verts * vertical_levels;

        const uint cap_radial = std::max<uint>(1, tci->cap_radial_segments);
        const uint cap_ring_count = gen_caps ? (cap_radial + 1) : 0; // rings per cap
        const uint cap_verts = gen_caps ? (ring_verts * cap_ring_count * 2) : 0; // bottom + top

        const uint numberVertices = outer_verts + inner_verts + cap_verts;

        const uint outer_indices = segments * stacks * 6;
        const uint inner_indices = segments * stacks * 6;
        const uint cap_indices = gen_caps ? (cap_radial * segments * 12) : 0;

        const uint numberIndices = outer_indices + inner_indices + cap_indices;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Tube", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return nullptr;

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(segments);

        // Outer surface: write vertices angle-major, then stack levels
        for (uint i = 0; i <= segments; i++)
        {
            float angle = angle_step * float(i);
            float cos_a = cos(angle);
            float sin_a = sin(angle);

            float nx = cos_a;
            float ny = sin_a;
            float nz = 0.0f;

            for (uint s = 0; s < vertical_levels; s++)
            {
                float z = -half_len + (float(s) / float(stacks)) * length;
                builder.WriteFullVertex(cos_a * outer_r, sin_a * outer_r, z,
                                       nx, ny, nz,
                                       -sin_a, cos_a, 0.0f,
                                       float(i) / float(segments), float(s) / float(stacks));
            }
        }

        // Inner surface (inverted normals)
        for (uint i = 0; i <= segments; i++)
        {
            float angle = angle_step * float(i);
            float cos_a = cos(angle);
            float sin_a = sin(angle);

            float nx = -cos_a;
            float ny = -sin_a;
            float nz = 0.0f;

            for (uint s = 0; s < vertical_levels; s++)
            {
                float z = -half_len + (float(s) / float(stacks)) * length;
                builder.WriteFullVertex(cos_a * inner_r, sin_a * inner_r, z,
                                       nx, ny, nz,
                                       -sin_a, cos_a, 0.0f,
                                       float(i) / float(segments), float(s) / float(stacks));
            }
        }

        // Caps (if enabled) - support radial subdivisions between inner and outer
        if (gen_caps)
        {
            const uint cap_ring_count = cap_radial + 1; // number of concentric rings per cap

            // Bottom cap rings (from inner -> outer)
            for (uint r = 0; r < cap_ring_count; r++)
            {
                float t = float(r) / float(cap_radial);
                float rr = inner_r + t * (outer_r - inner_r);

                for (uint i = 0; i <= segments; i++)
                {
                    float angle = angle_step * float(i);
                    float x = cos(angle) * rr;
                    float y = sin(angle) * rr;

                    // normal points down
                    builder.WriteFullVertex(x, y, -half_len,
                                           0.0f, 0.0f, -1.0f,
                                           1.0f, 0.0f, 0.0f,
                                           0.5f + x / (2.0f * outer_r), 0.5f + y / (2.0f * outer_r));
                }
            }

            // Top cap rings (from inner -> outer)
            for (uint r = 0; r < cap_ring_count; r++)
            {
                float t = float(r) / float(cap_radial);
                float rr = inner_r + t * (outer_r - inner_r);

                for (uint i = 0; i <= segments; i++)
                {
                    float angle = angle_step * float(i);
                    float x = cos(angle) * rr;
                    float y = sin(angle) * rr;

                    // normal points up
                    builder.WriteFullVertex(x, y, half_len,
                                           0.0f, 0.0f, 1.0f,
                                           1.0f, 0.0f, 0.0f,
                                           0.5f + x / (2.0f * outer_r), 0.5f + y / (2.0f * outer_r));
                }
            }
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Outer surface
            for (uint i = 0; i < segments; i++)
            {
                for (uint s = 0; s < stacks; s++)
                {
                    IndexT v00 = IndexT(i * vertical_levels + s);
                    IndexT v10 = IndexT((i + 1) * vertical_levels + s);
                    IndexT v01 = IndexT(i * vertical_levels + (s + 1));
                    IndexT v11 = IndexT((i + 1) * vertical_levels + (s + 1));

                    *ip++ = v00; *ip++ = v10; *ip++ = v11;
                    *ip++ = v00; *ip++ = v11; *ip++ = v01;
                }
            }

            // Inner surface (reverse winding)
            IndexT inner_base = outer_verts;
            for (uint i = 0; i < segments; i++)
            {
                for (uint s = 0; s < stacks; s++)
                {
                    IndexT v00 = inner_base + IndexT(i * vertical_levels + s);
                    IndexT v10 = inner_base + IndexT((i + 1) * vertical_levels + s);
                    IndexT v01 = inner_base + IndexT(i * vertical_levels + (s + 1));
                    IndexT v11 = inner_base + IndexT((i + 1) * vertical_levels + (s + 1));

                    // reversed winding so visible from inside
                    *ip++ = v00; *ip++ = v11; *ip++ = v10;
                    *ip++ = v00; *ip++ = v01; *ip++ = v11;
                }
            }

            // Caps
            if (gen_caps)
            {
                const uint cap_ring_count = cap_radial + 1; // rings per cap
                IndexT cap_base = outer_verts + inner_verts;
                // Bottom cap: rings 0..cap_ring_count-1 (inner->outer)
                for (uint r = 0; r < cap_radial; r++)
                {
                    IndexT ring0 = cap_base + IndexT(r * ring_verts);
                    IndexT ring1 = cap_base + IndexT((r + 1) * ring_verts);

                    for (uint i = 0; i < segments; i++)
                    {
                        IndexT a = ring0 + i;
                        IndexT b = ring1 + i;
                        IndexT c = ring0 + (i + 1);
                        IndexT d = ring1 + (i + 1);

                        // Bottom cap: normal = -Z, CCW when viewed from -Z
                        *ip++ = a; *ip++ = c; *ip++ = b;
                        *ip++ = b; *ip++ = c; *ip++ = d;
                    }
                }

                // Top cap: located after bottom cap rings
                IndexT top_base = cap_base + IndexT(cap_ring_count * ring_verts);
                for (uint r = 0; r < cap_radial; r++)
                {
                    IndexT ring0 = top_base + IndexT(r * ring_verts);
                    IndexT ring1 = top_base + IndexT((r + 1) * ring_verts);

                    for (uint i = 0; i < segments; i++)
                    {
                        IndexT a = ring0 + i;
                        IndexT b = ring0 + (i + 1);
                        IndexT c = ring1 + i;
                        IndexT d = ring1 + (i + 1);

                        // Top cap: normal = +Z, CCW when viewed from +Z
                        *ip++ = a; *ip++ = c; *ip++ = b;
                        *ip++ = c; *ip++ = d; *ip++ = b;
                    }
                }
            }
        };

        if(index_type == IndexType::U16)
        {
            auto ib = pc->GetIndexAccessor<uint16>();
            uint16 *ip = ib;
            generate_indices(ip);
        }
        else if(index_type == IndexType::U32)
        {
            auto ib = pc->GetIndexAccessor<uint32>();
            uint32 *ip = ib;
            generate_indices(ip);
        }
        else if(index_type == IndexType::U8)
        {
            auto ib = pc->GetIndexAccessor<uint8>();
            uint8 *ip = ib;
            generate_indices(ip);
        }
        else
            return nullptr;

        return pc->CreateWithAABB(
            math::Vector3f(-outer_r, -outer_r, -half_len),
            Vector3f(outer_r, outer_r, half_len));
    }
} // namespace hgl::graph::inline_geometry
