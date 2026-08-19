// Rounded box geometry generator for ULRE engine
// Creates a box with rounded edges and corners

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    /**
     * 圆角CUBE，但存在BUG，外平面使用的是整个CUBE尺寸，没有减去圆角部分。两个顶角之间的弧形过度也没有生成
     */
    Geometry *CreateRoundedBox(GeometryCreater *pc, const RoundedBoxCreateInfo *rbci)
    {
        if(!pc || !rbci)
            return nullptr;

        const float sx = rbci->size.x * 0.5f;
        const float sy = rbci->size.y * 0.5f;
        const float sz = rbci->size.z * 0.5f;
        const float r = rbci->edge_radius;
        const uint edge_segs = std::max<uint>(1, rbci->edge_segments);
        const uint face_segs = std::max<uint>(1, rbci->face_segments);

        // Validate edge_radius doesn't exceed half the size
        float min_half_size = std::min({2.0f*sx, 2.0f*sy, 2.0f*sz}) * 0.5f;
        if(r > min_half_size)
            return nullptr;

        // Inner box dimensions (corners of box minus radius)
        const float ix = sx - r;
        const float iy = sy - r;
        const float iz = sz - r;

        uint verts_per_corner = (edge_segs + 1) * (edge_segs + 1);
        uint verts_per_face = (face_segs + 1) * (face_segs + 1);

        // 8 corners + 6 faces
        uint numberVertices = 8 * verts_per_corner + 6 * verts_per_face;
        uint numberIndices = 8 * edge_segs * edge_segs * 6 +  // corners
                            6 * face_segs * face_segs * 6;      // faces

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("RoundedBox", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return nullptr;

        // Corner sphere octants
        struct Corner { int x, y, z; Vector3f center; };
        const Corner corners[8] = {
            {-1, -1, -1, Vector3f(-ix, -iy, -iz)},
            { 1, -1, -1, Vector3f( ix, -iy, -iz)},
            { 1,  1, -1, Vector3f( ix,  iy, -iz)},
            {-1,  1, -1, Vector3f(-ix,  iy, -iz)},
            {-1, -1,  1, Vector3f(-ix, -iy,  iz)},
            { 1, -1,  1, Vector3f( ix, -iy,  iz)},
            { 1,  1,  1, Vector3f( ix,  iy,  iz)},
            {-1,  1,  1, Vector3f(-ix,  iy,  iz)},
        };

        uint vertex_idx = 0;
        uint corner_base[8];

        const float pi_half = 1.5707963267948966f; // π/2

        // Generate corner vertices
        for(uint c = 0; c < 8; c++)
        {
            corner_base[c] = vertex_idx;
            const Corner& corner = corners[c];

            for(uint i = 0; i <= edge_segs; i++)
            {
                float theta = (float(i) / float(edge_segs)) * pi_half;
                float cos_t = cosf(theta);
                float sin_t = sinf(theta);

                for(uint j = 0; j <= edge_segs; j++)
                {
                    float phi = (float(j) / float(edge_segs)) * pi_half;
                    float cos_p = cosf(phi);
                    float sin_p = sinf(phi);

                    // Spherical patch point
                    float dx = sin_t * cos_p * corner.x;
                    float dy = sin_t * sin_p * corner.y;
                    float dz = cos_t * corner.z;

                    float px = corner.center.x + dx * r;
                    float py = corner.center.y + dy * r;
                    float pz = corner.center.z + dz * r;

                    // Normal (outward from sphere center)
                    float len = sqrtf(dx*dx + dy*dy + dz*dz);
                    float nx = (len > 0) ? dx/len : 0;
                    float ny = (len > 0) ? dy/len : 0;
                    float nz = (len > 0) ? dz/len : 0;

                    builder.WriteFullVertex(px, py, pz, nx, ny, nz, 1, 0, 0, float(i)/edge_segs, float(j)/edge_segs);
                }
            }
            vertex_idx += verts_per_corner;
        }

        // Generate flat face vertices
        uint face_base[6];

        // Face info: normal, and two edge vectors
        struct FaceInfo {
            Vector3f normal;
            Vector3f u_dir;    // local u direction
            Vector3f v_dir;    // local v direction
            Vector3f origin;   // inner box corner
            Vector3f u_ext;    // distance to outer edge (u direction)
            Vector3f v_ext;    // distance to outer edge (v direction)
        };

        FaceInfo faces[6] = {
            // -Z face
            {Vector3f(0,0,-1), Vector3f(1,0,0), Vector3f(0,1,0), Vector3f(-sx,-sy,-sz), Vector3f(2*sx,0,0), Vector3f(0,2*sy,0)},
            // +Z face
            {Vector3f(0,0,1), Vector3f(1,0,0), Vector3f(0,1,0), Vector3f(-sx,-sy,sz), Vector3f(2*sx,0,0), Vector3f(0,2*sy,0)},
            // -Y face
            {Vector3f(0,-1,0), Vector3f(1,0,0), Vector3f(0,0,1), Vector3f(-sx,-sy,-sz), Vector3f(2*sx,0,0), Vector3f(0,0,2*sz)},
            // +Y face
            {Vector3f(0,1,0), Vector3f(1,0,0), Vector3f(0,0,1), Vector3f(-sx,sy,-sz), Vector3f(2*sx,0,0), Vector3f(0,0,2*sz)},
            // -X face
            {Vector3f(-1,0,0), Vector3f(0,1,0), Vector3f(0,0,1), Vector3f(-sx,-sy,-sz), Vector3f(0,2*sy,0), Vector3f(0,0,2*sz)},
            // +X face
            {Vector3f(1,0,0), Vector3f(0,1,0), Vector3f(0,0,1), Vector3f(sx,-sy,-sz), Vector3f(0,2*sy,0), Vector3f(0,0,2*sz)},
        };

        for(uint f = 0; f < 6; f++)
        {
            face_base[f] = vertex_idx;
            const FaceInfo& face = faces[f];

            for(uint i = 0; i <= face_segs; i++)
            {
                float t_u = float(i) / float(face_segs);

                for(uint j = 0; j <= face_segs; j++)
                {
                    float t_v = float(j) / float(face_segs);

                    // Position: origin + (u_ext * t_u) + (v_ext * t_v)
                    Vector3f pos = face.origin + face.u_ext * t_u + face.v_ext * t_v;

                    builder.WriteFullVertex(pos.x, pos.y, pos.z,
                                          face.normal.x, face.normal.y, face.normal.z,
                                          face.u_dir.x, face.u_dir.y, face.u_dir.z,
                                          t_u, t_v);
                }
            }
            vertex_idx += verts_per_face;
        }

        // Generate indices
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Corner indices
            for(uint c = 0; c < 8; c++)
            {
                IndexT base = corner_base[c];
                for(uint i = 0; i < edge_segs; i++)
                {
                    for(uint j = 0; j < edge_segs; j++)
                    {
                        IndexT v0 = base + i * (edge_segs + 1) + j;
                        IndexT v1 = base + i * (edge_segs + 1) + j + 1;
                        IndexT v2 = base + (i + 1) * (edge_segs + 1) + j;
                        IndexT v3 = base + (i + 1) * (edge_segs + 1) + j + 1;

                        *ip++ = v0;
                        *ip++ = v2;
                        *ip++ = v1;
                        *ip++ = v1;
                        *ip++ = v2;
                        *ip++ = v3;
                    }
                }
            }

            // Face indices
            for(uint f = 0; f < 6; f++)
            {
                IndexT base = face_base[f];
                for(uint i = 0; i < face_segs; i++)
                {
                    for(uint j = 0; j < face_segs; j++)
                    {
                        IndexT v0 = base + i * (face_segs + 1) + j;
                        IndexT v1 = base + i * (face_segs + 1) + j + 1;
                        IndexT v2 = base + (i + 1) * (face_segs + 1) + j;
                        IndexT v3 = base + (i + 1) * (face_segs + 1) + j + 1;

                        *ip++ = v0;
                        *ip++ = v2;
                        *ip++ = v1;
                        *ip++ = v1;
                        *ip++ = v2;
                        *ip++ = v3;
                    }
                }
            }
        };

        if (index_type == IndexType::U32) {
            auto ib = pc->GetIndexAccessor<uint32>();
            uint32 *ip = ib;
            generate_indices(ip);
        }else
            return nullptr;

        return pc->CreateWithAABB(
            Vector3f(-sx, -sy, -sz),
            Vector3f(sx, sy, sz));
    }
} // namespace hgl::graph::inline_geometry
