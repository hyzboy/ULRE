#include "InlineGeometryCommon.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    struct DomeTri
    {
        uint a, b, c;
    };

    static uint EstimateIcoSubdivisionFromSlices(const uint numberSlices)
    {
        uint level = 0;
        uint target = 8;

        while (target < numberSlices && level < 6)
        {
            ++level;
            target <<= 1;
        }

        return level;
    }

    static uint64_t MakeEdgeKey(uint a, uint b)
    {
        const uint lo = std::min(a, b);
        const uint hi = std::max(a, b);
        return (uint64_t(lo) << 32) | uint64_t(hi);
    }

    Geometry *CreateDome(GeometryCreater *pc, const DomeCreateInfo *dci)
    {
        if (!pc || !dci)
            return nullptr;

        const uint numberSlices = dci->number_slices;
        const bool inside_out = dci->inside_out;

        if (!GeometryValidator::ValidateSlices(numberSlices, 3))
            return nullptr;

        std::vector<Vector3f> sphere_vertices;
        std::vector<DomeTri> sphere_tris;

        sphere_vertices.reserve(4096);
        sphere_tris.reserve(8192);

        const float t = (1.0f + sqrtf(5.0f)) * 0.5f;

        auto add_vertex = [&](float x, float y, float z)
        {
            sphere_vertices.emplace_back(glm::normalize(Vector3f(x, y, z)));
        };

        // Icosahedron base vertices
        add_vertex(-1,  t,  0); add_vertex( 1,  t,  0); add_vertex(-1, -t,  0); add_vertex( 1, -t,  0);
        add_vertex( 0, -1,  t); add_vertex( 0,  1,  t); add_vertex( 0, -1, -t); add_vertex( 0,  1, -t);
        add_vertex( t,  0, -1); add_vertex( t,  0,  1); add_vertex(-t,  0, -1); add_vertex(-t,  0,  1);

        auto add_tri = [&](uint a, uint b, uint c)
        {
            sphere_tris.push_back({a, b, c});
        };

        add_tri(0,11,5);  add_tri(0,5,1);   add_tri(0,1,7);   add_tri(0,7,10);  add_tri(0,10,11);
        add_tri(1,5,9);   add_tri(5,11,4);  add_tri(11,10,2); add_tri(10,7,6);  add_tri(7,1,8);
        add_tri(3,9,4);   add_tri(3,4,2);   add_tri(3,2,6);   add_tri(3,6,8);   add_tri(3,8,9);
        add_tri(4,9,5);   add_tri(2,4,11);  add_tri(6,2,10);  add_tri(8,6,7);   add_tri(9,8,1);

        const uint subdivision = EstimateIcoSubdivisionFromSlices(numberSlices);
        std::unordered_map<uint64_t, uint> midpoint_cache;
        midpoint_cache.reserve(8192);

        auto get_midpoint = [&](uint a, uint b) -> uint
        {
            const uint64_t key = MakeEdgeKey(a, b);
            auto iter = midpoint_cache.find(key);
            if (iter != midpoint_cache.end())
                return iter->second;

            Vector3f mid = glm::normalize(sphere_vertices[a] + sphere_vertices[b]);
            const uint idx = uint(sphere_vertices.size());
            sphere_vertices.push_back(mid);
            midpoint_cache.emplace(key, idx);
            return idx;
        };

        for (uint s = 0; s < subdivision; ++s)
        {
            midpoint_cache.clear();

            std::vector<DomeTri> refined;
            refined.reserve(sphere_tris.size() * 4);

            for (const DomeTri &tri : sphere_tris)
            {
                const uint ab = get_midpoint(tri.a, tri.b);
                const uint bc = get_midpoint(tri.b, tri.c);
                const uint ca = get_midpoint(tri.c, tri.a);

                refined.push_back({tri.a, ab, ca});
                refined.push_back({tri.b, bc, ab});
                refined.push_back({tri.c, ca, bc});
                refined.push_back({ab, bc, ca});
            }

            sphere_tris.swap(refined);
        }

        std::vector<DomeTri> dome_tris;
        dome_tris.reserve(sphere_tris.size() / 2);

        // Keep only upper hemisphere triangles to form a dome.
        for (const DomeTri &tri : sphere_tris)
        {
            const Vector3f &a = sphere_vertices[tri.a];
            const Vector3f &b = sphere_vertices[tri.b];
            const Vector3f &c = sphere_vertices[tri.c];

            if (a.z >= 0.0f && b.z >= 0.0f && c.z >= 0.0f)
                dome_tris.push_back(tri);
        }

        if (dome_tris.empty())
            return nullptr;

        std::unordered_map<uint, uint> remap;
        remap.reserve(dome_tris.size());

        std::vector<Vector3f> dome_vertices;
        dome_vertices.reserve(dome_tris.size());

        std::vector<DomeTri> dome_indices;
        dome_indices.reserve(dome_tris.size());

        auto remap_vertex = [&](uint old_index) -> uint
        {
            auto it = remap.find(old_index);
            if (it != remap.end())
                return it->second;

            const uint new_index = uint(dome_vertices.size());
            remap.emplace(old_index, new_index);
            dome_vertices.push_back(sphere_vertices[old_index]);
            return new_index;
        };

        for (const DomeTri &tri : dome_tris)
        {
            const uint a = remap_vertex(tri.a);
            const uint b = remap_vertex(tri.b);
            const uint c = remap_vertex(tri.c);

            if (!inside_out)
                dome_indices.push_back({a, b, c});
            else
                dome_indices.push_back({a, c, b});
        }

        const uint numberVertices = uint(dome_vertices.size());
        const uint numberIndices = uint(dome_indices.size() * 3);

        if (!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if (!pc->Init("Dome", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        if (!builder.IsValid())
            return nullptr;

        const bool write_normal = (dci->normal != VK_FORMAT_UNDEFINED) && builder.HasNormals();
        const bool write_tangent = (dci->normal != VK_FORMAT_UNDEFINED)
                    && (dci->tangent != VK_FORMAT_UNDEFINED)
                    && builder.HasTangents();
        const bool write_tex_coord = (dci->tex_coord != VK_FORMAT_UNDEFINED) && builder.HasTexCoords();

        const float half_pi = std::numbers::pi_v<float> * 0.5f;

        for (const Vector3f &p : dome_vertices)
        {
            const Vector3f n = glm::normalize(p);

            const float theta = atan2f(n.y, n.x);                          // [-pi, pi]
            const float phi = acosf(std::clamp(n.z, -1.0f, 1.0f));         // [0, pi/2] on dome
            const float r = std::clamp(phi / half_pi, 0.0f, 1.0f);         // map zenith->0, horizon->1

            // Circular sky map packed in a square texture.
            const float u = 0.5f + 0.5f * r * cosf(theta);
            const float v = 0.5f + 0.5f * r * sinf(theta);

            builder.WriteVertex(p.x, p.y, p.z);

            if (write_normal)
                builder.WriteNormal(n.x, n.y, n.z);

            if (write_tangent)
            {
                Vector3f tdir(-sinf(theta), cosf(theta), 0.0f);
                if (glm::length(tdir) < 1e-6f)
                    tdir = Vector3f(1.0f, 0.0f, 0.0f);

                tdir = glm::normalize(tdir - n * Dot(n, tdir));
                builder.WriteTangent(tdir.x, tdir.y, tdir.z);
            }

            if (write_tex_coord)
                builder.WriteTexCoord(u, v);
        }

        const IndexType index_type = pc->GetIndexType();

        if (index_type == IndexType::U16)
        {
            auto ib = pc->GetIndexAccessor<uint16>();
            uint16 *ip = ib;
            for (const DomeTri &tri : dome_indices)
            {
                *ip++ = uint16(tri.a);
                *ip++ = uint16(tri.b);
                *ip++ = uint16(tri.c);
            }
        }
        else if (index_type == IndexType::U32)
        {
            auto ib = pc->GetIndexAccessor<uint32>();
            uint32 *ip = ib;
            for (const DomeTri &tri : dome_indices)
            {
                *ip++ = uint32(tri.a);
                *ip++ = uint32(tri.b);
                *ip++ = uint32(tri.c);
            }
        }
        else if (index_type == IndexType::U8)
        {
            auto ib = pc->GetIndexAccessor<uint8>();
            uint8 *ip = ib;
            for (const DomeTri &tri : dome_indices)
            {
                *ip++ = uint8(tri.a);
                *ip++ = uint8(tri.b);
                *ip++ = uint8(tri.c);
            }
        }
        else
        {
            return nullptr;
        }

        return pc->CreateWithAABB(
            math::Vector3f(-1.0f, -1.0f, 0.0f),
            math::Vector3f(1.0f, 1.0f, 1.0f));
    }
}
