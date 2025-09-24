// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    Geometry *CreateHexSphere(GeometryCreater *pc,const HexSphereCreateInfo *hsci)
    {
        if(!pc||!hsci) return nullptr;

        // 生成基础二十面体
        struct Tri { uint a,b,c; };
        std::vector<Vector3f> verts;
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

        // 索引缓存中间点
        struct EdgeKey { uint a,b; bool operator==(const EdgeKey& o)const{return a==o.a&&b==o.b;} };
        struct EdgeHash { size_t operator()(const EdgeKey& k)const { return (size_t(k.a)<<32) ^ k.b; } };
        std::unordered_map<EdgeKey,uint,EdgeHash> midpoint;

        auto get_mid = [&](uint a,uint b){
            EdgeKey key{std::min(a,b),std::max(a,b)};
            auto it = midpoint.find(key);
            if(it!=midpoint.end()) return it->second;
            Vector3f m = verts[a]+verts[b]; m = glm::normalize(m);
            uint id = (uint)verts.size(); verts.push_back(m); midpoint.emplace(key,id); return id;
            };

        // 细分
        for(uint s=0;s<hsci->subdivisions;s++)
        {
            std::vector<Tri> ntris; ntris.reserve(tris.size()*4);
            for(const auto &t : tris)
            {
                uint ab = get_mid(t.a,t.b);
                uint bc = get_mid(t.b,t.c);
                uint ca = get_mid(t.c,t.a);
                // 保持顺时针正面（a,b,c 为外观时顺时针），四分三角形
                ntris.push_back({t.a, ab, ca});
                ntris.push_back({t.b, bc, ab});
                ntris.push_back({t.c, ca, bc});
                ntris.push_back({ab, bc, ca});
            }
            tris.swap(ntris);
        }

        // 归一化到半径
        const float R = hsci->radius;
        for(auto &v:verts) v *= R;

        const uint vertex_count = (uint)verts.size();
        const uint index_count  = (uint)tris.size()*3;

        if(!pc->Init("HexSphere", vertex_count, index_count))
            return nullptr;

        VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm(pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tan(pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat uv (pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos;
        float *np = nrm;
        float *tp = tan;
        float *uvp= uv;

        if(!vp) return nullptr;

        // 写顶点属性：法线=单位方向，切线取经向方向（在极点退化时给固定值）
        for(const auto &v:verts)
        {
            *vp++ = v.x; *vp++ = v.y; *vp++ = v.z;

            if(np)
            {
                Vector3f n = glm::normalize(v);
                *np++ = n.x; *np++ = n.y; *np++ = n.z;
            }

            if(uvp)
            {
                Vector3f n = glm::normalize(v);
                // 球面 UV，经度[-pi,pi] -> u in [0,1]，纬度[-pi/2,pi/2] -> v in [0,1]
                float u = (atan2f(n.y, n.x) / (2.0f*HGL_PI)) + 0.5f;
                float vtex = (asinf(std::clamp(n.z, -1.0f, 1.0f))/HGL_PI) + 0.5f;
                *uvp++ = u * hsci->uv_scale.x;
                *uvp++ = vtex * hsci->uv_scale.y;
            }

            if(tp)
            {
                Vector3f n = glm::normalize(v);
                // 经向切线：沿 +theta（绕Z）方向近似：(-y, x, 0) 并去掉与 n 的投影
                Vector3f tdir(-n.y, n.x, 0.0f);
                if(glm::length(tdir)<1e-6f) tdir = Vector3f(1,0,0); // 极点备选
                tdir = (tdir - n * dot(n, tdir));
                tdir = glm::normalize(tdir);
                *tp++ = tdir.x; *tp++ = tdir.y; *tp++ = tdir.z;
            }
        }

        // 索引：顺时针为正面，直接按 tris 中的 (a,b,c) 顺序写
        {
            IBMap *ib = pc->GetIBMap();
            const IndexType it = pc->GetIndexType();
            if(it==IndexType::U16)
            {
                IBTypeMap<uint16> im(ib);
                uint16 *ip = im;
                for(const auto &t : tris){ *ip++=(uint16)t.a; *ip++=(uint16)t.b; *ip++=(uint16)t.c; }
            }
            else if(it==IndexType::U32)
            {
                IBTypeMap<uint32> im(ib);
                uint32 *ip = im;
                for(const auto &t : tris){ *ip++=t.a; *ip++=t.b; *ip++=t.c; }
            }
            else if(it==IndexType::U8)
            {
                IBTypeMap<uint8> im(ib);
                uint8 *ip = im;
                for(const auto &t : tris){ *ip++=(uint8)t.a; *ip++=(uint8)t.b; *ip++=(uint8)t.c; }
            }
            else return nullptr;
        }

        Geometry *p = pc->Create();
        if(p)
            p->SetBoundingBox(Vector3f(-R,-R,-R), Vector3f(R,R,R));
        return p;
    }
}//namespace hgl::graph::inline_geometry
