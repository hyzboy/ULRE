// Teardrop geometry generator for ULRE engine
// Creates a 3D teardrop shape using surface of revolution

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateTeardrop(GeometryCreater *pc, const TeardropCreateInfo *tci)
    {
        if(!pc || !tci)
            return nullptr;

        const float radius = tci->radius;
        const float length = tci->length;
        const uint segments = std::max<uint>(8, tci->segments);
        const uint radial_segs = std::max<uint>(8, tci->radial_segments);

        // Validate parameters
        if(radius <= 0.0f || length <= 0.0f)
            return nullptr;

        // Teardrop profile: circular bottom + tapered top
        // Profile points from bottom (0) to tip (length)
        const uint profile_points = segments + 1;

        // Vertices: profile_points * (radial_segs + 1) + tip vertex
        const uint numberVertices = profile_points * (radial_segs + 1) + 1;

        // Indices: (profile_points - 1) * radial_segs * 6
        const uint numberIndices = (profile_points - 1) * radial_segs * 6;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Teardrop", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Generate profile curve
        // Use a parametric curve that creates teardrop shape
        auto teardrop_profile = [&](float t) -> std::pair<float, float>
        {
            // t from 0 to 1
            // Returns (r, z) where r is distance from axis, z is height
            
            if(t < 0.5f)
            {
                // Bottom half: circular
                float theta = t * std::numbers::pi_v<float>;
                float r = radius * sin(theta);
                float z = radius * (1.0f - cos(theta));
                return {r, z};
            }
            else
            {
                // Top half: taper to point
                float s = (t - 0.5f) * 2.0f;  // 0 to 1
                float r = radius * (1.0f - s);
                float z = radius + (length - radius) * s;
                return {r, z};
            }
        };

        const float angle_step = (2.0f * std::numbers::pi_v<float>) / float(radial_segs);

        // Generate vertices
        for(uint i = 0; i <= profile_points; i++)
        {
            float t = float(i) / float(segments);
            auto [r, z] = teardrop_profile(std::min(t, 1.0f));

            for(uint j = 0; j <= radial_segs; j++)
            {
                float angle = angle_step * float(j);
                float x = cos(angle) * r;
                float y = sin(angle) * r;

                // Calculate normal (numerical differentiation for profile curve)
                float dt = 0.01f;
                auto [r_prev, z_prev] = teardrop_profile(std::max(0.0f, t - dt));
                auto [r_next, z_next] = teardrop_profile(std::min(1.0f, t + dt));

                // Tangent along profile
                float dr = r_next - r_prev;
                float dz = z_next - z_prev;
                float len = sqrtf(dr * dr + dz * dz);
                if(len > 0.0f) { dr /= len; dz /= len; }

                // Normal perpendicular to profile (in rz plane)
                float nr = dz;
                float nz = -dr;

                // Transform to 3D
                float nx = cos(angle) * nr;
                float ny = sin(angle) * nr;

                // Tangent along revolution
                float tx = -sin(angle);
                float ty = cos(angle);
                float tz = 0.0f;

                float u = float(j) / float(radial_segs);
                float v = t;

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

            for(uint i = 0; i < profile_points; i++)
            {
                for(uint j = 0; j < radial_segs; j++)
                {
                    IndexT v0 = i * (radial_segs + 1) + j;
                    IndexT v1 = i * (radial_segs + 1) + j + 1;
                    IndexT v2 = (i + 1) * (radial_segs + 1) + j;
                    IndexT v3 = (i + 1) * (radial_segs + 1) + j + 1;

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
        BoundingVolumes bv;
        bv.SetFromAABB(math::Vector3f(-radius, -radius, 0.0f),
                       Vector3f(radius, radius, length));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
