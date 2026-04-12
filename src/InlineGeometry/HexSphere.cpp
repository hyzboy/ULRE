// Sphere, cylinder, cone, and torus code adapted from McNopper:
// https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/geo/InlineGeometry.h>
#include <hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GeometryBuilder.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/type/UnorderedMap.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateHexSphere(GeometryCreater *pc,const HexSphereCreateInfo *hsci)
    {
        if(!pc||!hsci) return nullptr;

        // Generate a base icosahedron.
        struct Tri { uint a,b,c; };
        std::vector<math::Vector3f> verts;
        std::vector<Tri> tris;

        auto add = [&](float x,float y,float z){ verts.emplace_back(x,y,z); };

        const float t = (1.0f + sqrtf(5.0f)) * 0.5f; // golden ratio
        // 12 vertices of icosahedron
        add(-1,  t,  0); add( 1,  t,  0); add(-1, -t,  0); add( 1, -t,  0);
        add( 0, -1,  t); add( 0,  1,  t); add( 0, -1, -t); add( 0,  1, -t);
        add( t,  0, -1); add( t,  0,  1); add(-t,  0, -1); add(-t,  0,  1);

        for(auto &v:verts) v = glm::normalize(v);

        auto push = [&](uint a,uint b,uint c){ tris.push_back({a,b,c}); };
        // 20 faces; ensure Z-up and prefer clockwise front faces when looking from outside.
        push(0,11,5); push(0,5,1); push(0,1,7); push(0,7,10); push(0,10,11);
        push(1,5,9); push(5,11,4); push(11,10,2); push(10,7,6); push(7,1,8);
        push(3,9,4); push(3,4,2); push(3,2,6); push(3,6,8); push(3,8,9);
        push(4,9,5); push(2,4,11); push(6,2,10); push(8,6,7); push(9,8,1);

        // Midpoint cache for edge subdivision.
        struct EdgeKey { uint a,b; bool operator==(const EdgeKey& o)const{return a==o.a&&b==o.b;} };
        struct EdgeHash { size_t operator()(const EdgeKey& k)const { return (size_t(k.a)<<32) ^ k.b; } };
        hgl::UnorderedMap<EdgeKey,uint,EdgeHash> midpoint;

        auto get_mid = [&](uint a,uint b){
            EdgeKey key{std::min(a,b),std::max(a,b)};
            if (auto value = midpoint.GetValuePointer(key))
                return *value;
            Vector3f m = verts[a]+verts[b]; m = glm::normalize(m);
            uint id = (uint)verts.size(); verts.push_back(m); midpoint.Add(key,id); return id;
            };

        // Subdivide each triangle.
        for(uint s=0;s<hsci->subdivisions;s++)
        {
            std::vector<Tri> ntris; ntris.reserve(tris.size()*4);
            for(const auto &t : tris)
            {
                uint ab = get_mid(t.a,t.b);
                uint bc = get_mid(t.b,t.c);
                uint ca = get_mid(t.c,t.a);
                // Keep clockwise winding for front faces and split into 4 triangles.
                ntris.push_back({t.a, ab, ca});
                ntris.push_back({t.b, bc, ab});
                ntris.push_back({t.c, ca, bc});
                ntris.push_back({ab, bc, ca});
            }
            tris.swap(ntris);
        }

        // Scale vertices to the requested radius.
        const float R = hsci->radius;
        for(auto &v:verts) v *= R;

        const uint vertex_count = (uint)verts.size();
        const uint index_count  = (uint)tris.size()*3;

        if(!pc->Init("HexSphere", vertex_count, index_count))
            return nullptr;

        GeometryBuilder builder(pc);

        if(!builder.IsValid())
            return nullptr;

        // Write vertex attributes: normal is the unit direction from center,
        // tangent uses longitude direction with a pole fallback.
        for(const auto &v:verts)
        {
            builder.WriteVertex(v.x, v.y, v.z);

            if(builder.HasNormals())
            {
                Vector3f n = glm::normalize(v);
                builder.WriteNormal(n.x,n.y,n.z);
            }

            if(builder.HasTexCoords())
            {
                Vector3f n = glm::normalize(v);
                // Spherical UV mapping:
                // longitude [-pi, pi] -> u in [0, 1], latitude [-pi/2, pi/2] -> v in [0, 1].
                float u = (atan2f(n.y, n.x) / (2.0f*std::numbers::pi_v<float>)) + 0.5f;
                float vtex = (asinf(std::clamp(n.z, -1.0f, 1.0f))/std::numbers::pi_v<float>) + 0.5f;
                builder.WriteTexCoord(u * hsci->uv_scale.x,
                                      vtex * hsci->uv_scale.y);
            }

            if(builder.HasTangents())
            {
                Vector3f n = glm::normalize(v);
                // Longitudinal tangent: approximate +theta around Z with (-y, x, 0),
                // then remove projection onto normal n.
                Vector3f tdir(-n.y, n.x, 0.0f);
                if(glm::length(tdir)<1e-6f) tdir = Vector3f(1,0,0); // Pole fallback.
                tdir = (tdir - n * Dot(n, tdir));
                tdir = glm::normalize(tdir);
                builder.WriteTangent(tdir.x,tdir.y,tdir.z);
            }
        }

        // Indices: clockwise winding is front-facing; use (a, b, c) order directly.
        {
            const IndexType it = pc->GetIndexType();
            if(it==IndexType::U16)
            {
                auto im = pc->GetIndexAccessor<uint16>();
                uint16 *ip = im;
                for(const auto &t : tris){ *ip++=(uint16)t.a; *ip++=(uint16)t.b; *ip++=(uint16)t.c; }
            }
            else if(it==IndexType::U32)
            {
                auto im = pc->GetIndexAccessor<uint32>();
                uint32 *ip = im;
                for(const auto &t : tris){ *ip++=t.a; *ip++=t.b; *ip++=t.c; }
            }
            else if(it==IndexType::U8)
            {
                auto im = pc->GetIndexAccessor<uint8>();
                uint8 *ip = im;
                for(const auto &t : tris){ *ip++=(uint8)t.a; *ip++=(uint8)t.b; *ip++=(uint8)t.c; }
            }
            else return nullptr;
        }

        Geometry *p = pc->Create();
        if(p)
        {
            BoundingVolumes bv;
            bv.SetFromAABB(math::Vector3f(-R, -R, -R),
                          Vector3f(R, R, R));
            p->SetBoundingVolumes(bv);
        }
        return p;
    }
}//namespace hgl::graph::inline_geometry

