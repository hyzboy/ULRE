// sphere、cylinear、cone、tours code from McNopper,website: https://github.com/McNopper/GLUS
// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/geo/InlineGeometry.h>
#include <hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/geo/GeometryCreater.h>
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

        // 生成基础二十面体
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

        // ç´¢å¼ç¼å­ä¸­é´ç?
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

        auto pos = pc->GetTypedArrayView<TypedArrayView3f>(VAN::Position);
        auto nrm = pc->GetTypedArrayView<TypedArrayView3f>(VAN::Normal);
        auto tan = pc->GetTypedArrayView<TypedArrayView3f>(VAN::Tangent);

        if(!pos.IsValid())
            return nullptr;

        // RG16F/RG8 压缩法线（octahedral）：Normal VAB 为压缩格式时用 2 分量编码
        VAB *nrm_vab = pc->GetVAB(VAN::Normal);
        const bool nrm_rg8  = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R8G8_UNORM);
        const bool nrm_rg16f = (nrm_vab && nrm_vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT);
        TypedArrayView2u8 nrm2u8 = nrm_rg8  ? pc->GetTypedArrayView<TypedArrayView2u8>(VAN::Normal) : TypedArrayView2u8();
        TypedArrayView2hf nrm2   = nrm_rg16f ? pc->GetTypedArrayView<TypedArrayView2hf>(VAN::Normal) : TypedArrayView2hf();

        // UV 压缩格式分派（与 GeometryBuilder 的写法一致）：
        // TexCoord 槽位的实际格式由外部传入的 GeometryVertexFormat 决定，可能是
        // RG16F(VF_V2HF) 也可能是 RG32F(VF_V2F)。GetTypedArrayView **不做格式校验**，
        // 用 TypedArrayView2f 往 RG16F 槽位写 float2 会按 4B/顶点的 stride 写入
        // 8B/顶点 —— 既越界覆盖后续属性，又把 half 数据按 float 解释，uv 全成乱值。
        VAB *uv_vab = pc->GetVAB(VAN::TexCoord);
        const bool uv_rg16f = (uv_vab && uv_vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT);
        TypedArrayView2hf uv2 = uv_rg16f      ? pc->GetTypedArrayView<TypedArrayView2hf>(VAN::TexCoord) : TypedArrayView2hf();
        TypedArrayView2f  uv1 = (uv_vab && !uv_rg16f) ? pc->GetTypedArrayView<TypedArrayView2f>(VAN::TexCoord) : TypedArrayView2f();

        // 写顶点属性：法线=单位方向，切线取经向方向（在极点退化时给固定值）
        for(const auto &v:verts)
        {
            pos->Write(v);

            if(nrm2u8.IsValid())
            {
                const Vector3f nn = glm::normalize(v);
                float p, q;
                EncodeOctahedralNormal(nn.x, nn.y, nn.z, p, q);
                nrm2u8->Write(QuantizeU8(p), QuantizeU8(q));
            }
            else if(nrm2.IsValid())
            {
                const Vector3f n = glm::normalize(v);
                float p, q;
                EncodeOctahedralNormal(n.x, n.y, n.z, p, q);
                nrm2->Write(FloatToHalf(p), FloatToHalf(q));
            }
            else if(nrm.IsValid())
            {
                Vector3f n = glm::normalize(v);
                nrm->Write(n);
            }

            if(uv2.IsValid() || uv1.IsValid())
            {
                Vector3f n = glm::normalize(v);
                // 球面 UV，经度[-pi,pi] -> u in [0,1]，纬度[-pi/2,pi/2] -> v in [0,1]
                float u = (atan2f(n.y, n.x) / (2.0f*std::numbers::pi_v<float>)) + 0.5f;
                float vtex = (asinf(std::clamp(n.z, -1.0f, 1.0f))/std::numbers::pi_v<float>) + 0.5f;
                const float tu = u * hsci->uv_scale.x;
                const float tv = vtex * hsci->uv_scale.y;

                if(uv2.IsValid())
                    uv2->Write(FloatToHalf(tu), FloatToHalf(tv));
                else
                    uv1->Write(Vector2f(tu, tv));
            }

            if(tan.IsValid())
            {
                Vector3f n = glm::normalize(v);
                // ç»ååçº¿ï¼æ²¿ +thetaï¼ç»Zï¼æ¹åè¿ä¼¼ï¼(-y, x, 0) å¹¶å»æä¸ n çæå½?
                Vector3f tdir(-n.y, n.x, 0.0f);
                if(glm::length(tdir)<1e-6f) tdir = Vector3f(1,0,0); // æç¹å¤é?
                tdir = (tdir - n * Dot(n, tdir));
                tdir = glm::normalize(tdir);
                tan->Write(tdir);
            }
        }

        // ç´¢å¼ï¼é¡ºæ¶éä¸ºæ­£é¢ï¼ç´æ¥æ?tris ä¸­ç (a,b,c) é¡ºåºå?
        {
            const IndexType it = pc->GetIndexType();
            if (pc->GetIndexType() == IndexType::U32) {
                auto im = pc->GetIndexAccessor<uint32>();
                uint32 *ip = im;
                for(const auto &t : tris){ *ip++=t.a; *ip++=t.b; *ip++=t.c; }
            }else return nullptr;
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

