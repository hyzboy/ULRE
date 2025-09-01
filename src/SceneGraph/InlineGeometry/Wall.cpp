#include<hgl/graph/geometry/Wall.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/PrimitiveCreater.h>

#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace hgl::graph::inline_geometry
{
    // helper: line intersection p + t*r and q + u*s. returns true if intersect (not parallel), sets t and u
    static bool LineLineIntersect(const Vector2f &p, const Vector2f &r, const Vector2f &q, const Vector2f &s, float &t, float &u)
    {
        float rxs = r.x * s.y - r.y * s.x;
        if (fabs(rxs) < 1e-8f) return false;
        Vector2f qp = Vector2f(q.x - p.x, q.y - p.y);
        t = (qp.x * s.y - qp.y * s.x) / rxs;
        u = (qp.x * r.y - qp.y * r.x) / rxs;
        return true;
    }

    // normalize safe
    static Vector2f Normalize2(const Vector2f &v)
    {
        float len = sqrt(v.x*v.x + v.y*v.y);
        if(len <= 1e-8f) return Vector2f(0,0);
        return Vector2f(v.x/len, v.y/len);
    }

    // compute triangle normal (not normalized)
    static Vector3f TriNormal(const Vector3f &A,const Vector3f &B,const Vector3f &C)
    {
        Vector3f AB(B.x-A.x, B.y-A.y, B.z-A.z);
        Vector3f AC(C.x-A.x, C.y-A.y, C.z-A.z);
        return Vector3f( AB.y*AC.z - AB.z*AC.y,
                         AB.z*AC.x - AB.x*AC.z,
                         AB.x*AC.y - AB.y*AC.x );
    }

    // normalize 3f
    static Vector3f Normalize3(const Vector3f &v)
    {
        float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if(len <= 1e-8f) return Vector3f(0,0,0);
        return Vector3f(v.x/len, v.y/len, v.z/len);
    }

    Primitive *CreateWallsFromLines2D(PrimitiveCreater *pc, const WallCreateInfo *wci)
    {
        if(!pc || !wci) return nullptr;

        const Vector2f* verts = wci->vertices;
        uint vcount = wci->vertexCount;

        std::vector<uint> segA, segB;

        if(wci->indices && wci->indexCount >= 2 && verts && vcount > 0)
        {
            for(uint i=0;i+1<wci->indexCount;i+=2)
            {
                uint a = wci->indices[i];
                uint b = wci->indices[i+1];
                if(a < vcount && b < vcount){ segA.push_back(a); segB.push_back(b); }
            }
        }
        else if(wci->lines && wci->lineCount > 0 && verts && vcount > 0)
        {
            for(uint i=0;i<wci->lineCount;i++)
            {
                const Line2D &l = wci->lines[i];
                if(l.a < vcount && l.b < vcount){ segA.push_back(l.a); segB.push_back(l.b); }
            }
        }
        else return nullptr;

        if(segA.empty()) return nullptr;

        // Build adjacency to order polyline (support open or closed)
        std::vector<std::vector<uint>> adj(vcount);
        for(size_t i=0;i<segA.size();++i)
        {
            uint a = segA[i], b = segB[i];
            adj[a].push_back(b);
            adj[b].push_back(a);
        }

        int start = -1;
        for(uint i=0;i<vcount;++i)
        {
            if(!adj[i].empty()){
                if(adj[i].size()==1){ start = (int)i; break; }
                if(start==-1) start = (int)i;
            }
        }

        if(start==-1) return nullptr;

        std::vector<uint> seq;
        seq.reserve(segA.size()+1);
        std::vector<char> visited(vcount,0);

        int curr = start;
        int prev = -1;
        while(curr!=-1)
        {
            seq.push_back((uint)curr);
            visited[curr]=1;

            int next = -1;
            for(uint nb: adj[curr])
            {
                if((int)nb==prev) continue;
                next = (int)nb; break;
            }

            prev = curr;
            curr = next;
            if(curr!=-1 && visited[curr]) break; // closed loop or visited
        }

        bool closed = false;
        if(!seq.empty()){
            uint first = seq.front(), last = seq.back();
            for(uint nb: adj[last]) if(nb==first) { closed=true; break; }
        }

        const float halfT = wci->thickness * 0.5f;
        const float halfH = wci->height * 0.5f;

        size_t nverts = seq.size();
        if(nverts<2) return nullptr;

        std::vector<Vector2f> dir(nverts- (closed?0:1));
        std::vector<Vector2f> nrm(nverts- (closed?0:1));
        std::vector<float> segLength(dir.size());

        for(size_t i=0;i<dir.size();++i)
        {
            Vector2f a = verts[ seq[i] ];
            Vector2f b = verts[ seq[i+1] ];
            Vector2f d(b.x - a.x, b.y - a.y);
            float len = sqrt(d.x*d.x + d.y*d.y);
            if(len<=1e-8f){ dir[i]=Vector2f(0,0); nrm[i]=Vector2f(0,0); segLength[i]=0; continue; }
            d.x/=len; d.y/=len;
            dir[i]=d;
            nrm[i]=Vector2f(-d.y, d.x);
            segLength[i]=len;
        }

        // accumulate length along polyline (for U coordinate)
        std::vector<float> accum(nverts);
        accum[0]=0.0f;
        for(size_t i=1;i<nverts;i++) accum[i]=accum[i-1] + segLength[i-1];

        // compute joins
        struct JoinPoint { Vector2f left; Vector2f right; bool left_ok,right_ok; std::vector<Vector2f> round_left; std::vector<Vector2f> round_right; };
        std::vector<JoinPoint> jp(nverts);

        auto cornerMode = wci->cornerJoin;
        float miterLimit = wci->miterLimit;
        uint roundSeg = std::max<uint>(3, wci->roundSegments);

        auto compute_join = [&](size_t vi)
        {
            bool hasPrev = (vi>0);
            bool hasNext = (vi+1< nverts);
            if(closed){ hasPrev = true; hasNext = true; }

            Vector2f p = verts[ seq[vi] ];

            if(!hasPrev || !hasNext)
            {
                Vector2f d = hasNext ? dir[vi] : dir[vi-1];
                Vector2f n = hasNext ? nrm[vi] : nrm[vi-1];
                jp[vi].left = Vector2f(p.x + n.x*halfT, p.y + n.y*halfT);
                jp[vi].right = Vector2f(p.x - n.x*halfT, p.y - n.y*halfT);
                jp[vi].left_ok = jp[vi].right_ok = true;
                return;
            }

            Vector2f d1 = dir[vi-1];
            Vector2f n1 = nrm[vi-1];
            Vector2f d2 = dir[vi];
            Vector2f n2 = nrm[vi];

            Vector2f p1 = Vector2f(p.x + n1.x*halfT, p.y + n1.y*halfT);
            Vector2f r1 = d1;
            Vector2f p2 = Vector2f(p.x + n2.x*halfT, p.y + n2.y*halfT);
            Vector2f r2 = d2;

            float t,u;
            bool ok_left = LineLineIntersect(p1,r1,p2,r2,t,u);
            Vector2f left_pt;
            if(ok_left)
            {
                left_pt = Vector2f(p1.x + r1.x*t, p1.y + r1.y*t);
                float miter_len = sqrt((left_pt.x - p.x)*(left_pt.x - p.x) + (left_pt.y - p.y)*(left_pt.y - p.y));
                if(miter_len > miterLimit * halfT && cornerMode == WallCreateInfo::CornerJoin::Miter)
                    ok_left = false;
            }

            Vector2f q1 = Vector2f(p.x - n1.x*halfT, p.y - n1.y*halfT);
            Vector2f q2 = Vector2f(p.x - n2.x*halfT, p.y - n2.y*halfT);
            bool ok_right = LineLineIntersect(q1,r1,q2,r2,t,u);
            Vector2f right_pt;
            if(ok_right)
            {
                right_pt = Vector2f(q1.x + r1.x*t, q1.y + r1.y*t);
                float miter_len = sqrt((right_pt.x - p.x)*(right_pt.x - p.x) + (right_pt.y - p.y)*(right_pt.y - p.y));
                if(miter_len > miterLimit * halfT && cornerMode == WallCreateInfo::CornerJoin::Miter)
                    ok_right = false;
            }

            if(ok_left && ok_right && cornerMode == WallCreateInfo::CornerJoin::Miter)
            {
                jp[vi].left = left_pt; jp[vi].right = right_pt; jp[vi].left_ok = jp[vi].right_ok = true; return;
            }

            jp[vi].left = p1; jp[vi].left_ok = true;
            jp[vi].right = q1; jp[vi].right_ok = true;

            if(cornerMode == WallCreateInfo::CornerJoin::Round)
            {
                float a1 = atan2(n1.y, n1.x);
                float a2 = atan2(n2.y, n2.x);
                float da = a2 - a1;
                while(da <= -HGL_PI) da += 2*HGL_PI;
                while(da > HGL_PI) da -= 2*HGL_PI;
                int segs = (int)roundSeg;
                jp[vi].round_left.clear(); jp[vi].round_right.clear();
                for(int s=0;s<=segs;s++)
                {
                    float tseg = (float)s/(float)segs;
                    float ang = a1 + da * tseg;
                    jp[vi].round_left.push_back(Vector2f(p.x + cos(ang)*halfT, p.y + sin(ang)*halfT));
                }
                a1 += (float)M_PI; a2 += (float)M_PI; da = a2 - a1;
                while(da <= -HGL_PI) da += 2*HGL_PI;
                while(da > HGL_PI) da -= 2*HGL_PI;
                for(int s=0;s<=segs;s++)
                {
                    float tseg = (float)s/(float)segs;
                    float ang = a1 + da * tseg;
                    jp[vi].round_right.push_back(Vector2f(p.x + cos(ang)*halfT, p.y + sin(ang)*halfT));
                }
            }
        };

        for(size_t i=0;i<nverts;++i) compute_join(i);

        std::vector<Vector2f> left_poly, right_poly;
        left_poly.reserve(nverts*2);
        right_poly.reserve(nverts*2);

        for(size_t i=0;i<nverts;++i)
        {
            if(!jp[i].round_left.empty()) for(auto &pt: jp[i].round_left) left_poly.push_back(pt); else left_poly.push_back(jp[i].left);
            if(!jp[i].round_right.empty()) for(auto &pt: jp[i].round_right) right_poly.push_back(pt); else right_poly.push_back(jp[i].right);
        }

        if(left_poly.size() != right_poly.size()) { left_poly.clear(); right_poly.clear(); for(size_t i=0;i<nverts;++i){ left_poly.push_back(jp[i].left); right_poly.push_back(jp[i].right); } }

        size_t m = left_poly.size();
        if(m < 2) return nullptr;

        // build final verts
        std::vector<Vector3f> finalVerts; finalVerts.reserve(m*4);
        // Also build UV array
        std::vector<Vector2f> finalUV; finalUV.reserve(m*4);

        for(size_t i=0;i<m;i++)
        {
            Vector2f L = left_poly[i];
            Vector2f R = right_poly[i];
            // bottom L
            finalVerts.push_back(Vector3f(L.x, L.y, -halfH));
            // bottom R
            finalVerts.push_back(Vector3f(R.x, R.y, -halfH));
            // top R
            finalVerts.push_back(Vector3f(R.x, R.y, halfH));
            // top L
            finalVerts.push_back(Vector3f(L.x, L.y, halfH));

            float u = 0.0f;
            float minDist = FLT_MAX; size_t bestIdx=0;
            for(size_t vi=0;vi<nverts;++vi)
            {
                Vector2f op = verts[ seq[vi] ];
                float dx = L.x - op.x, dy = L.y - op.y;
                float d = dx*dx + dy*dy;
                if(d<minDist){ minDist=d; bestIdx=vi; }
            }
            u = accum[bestIdx] * wci->uv_u_repeat_per_unit;

            float v0 = 0.0f;
            float v1 = wci->uv_tile_v;

            finalUV.push_back(Vector2f(u, v0));
            finalUV.push_back(Vector2f(u, v0));
            finalUV.push_back(Vector2f(u, v1));
            finalUV.push_back(Vector2f(u, v1));
        }

        // indices
        std::vector<uint32_t> finalIndices; finalIndices.reserve((m-(closed?0:1))*24 + 12);
        auto vertIndex = [&](size_t i, int corner)->uint32_t{ return (uint32_t)(i*4 + corner); };
        size_t segCount = closed? m : (m-1);
        for(size_t i=0;i<segCount;i++)
        {
            size_t ni = (i+1)%m;
            uint32_t v0 = vertIndex(i,0), v1 = vertIndex(ni,0), v2 = vertIndex(ni,1), v3 = vertIndex(i,1);
            finalIndices.push_back(v0); finalIndices.push_back(v1); finalIndices.push_back(v2);
            finalIndices.push_back(v0); finalIndices.push_back(v2); finalIndices.push_back(v3);

            uint32_t t0 = vertIndex(i,3), t1 = vertIndex(ni,3), t2 = vertIndex(ni,2), t3 = vertIndex(i,2);
            finalIndices.push_back(t0); finalIndices.push_back(t2); finalIndices.push_back(t1);
            finalIndices.push_back(t0); finalIndices.push_back(t3); finalIndices.push_back(t2);

            uint32_t l0 = vertIndex(i,0), l1 = vertIndex(i,3), l2 = vertIndex(ni,3), l3 = vertIndex(ni,0);
            finalIndices.push_back(l0); finalIndices.push_back(l1); finalIndices.push_back(l2);
            finalIndices.push_back(l0); finalIndices.push_back(l2); finalIndices.push_back(l3);

            uint32_t r0 = vertIndex(i,1), r1 = vertIndex(ni,1), r2 = vertIndex(ni,2), r3 = vertIndex(i,2);
            finalIndices.push_back(r0); finalIndices.push_back(r1); finalIndices.push_back(r2);
            finalIndices.push_back(r0); finalIndices.push_back(r2); finalIndices.push_back(r3);
        }

        // Add caps for open polyline (start and end)
        if(!closed)
        {
            // start cap (use vertices at i=0)
            uint32_t b0 = vertIndex(0,0), b1 = vertIndex(0,1), t1 = vertIndex(0,2), t0 = vertIndex(0,3);
            // two triangles (t0,t1,b1) and (t0,b1,b0)
            size_t capStartIdx = finalIndices.size();
            finalIndices.push_back(t0); finalIndices.push_back(t1); finalIndices.push_back(b1);
            finalIndices.push_back(t0); finalIndices.push_back(b1); finalIndices.push_back(b0);

            // ensure orientation: cap normal should align with -dir[0]
            if(!dir.empty()){
                Vector3f N = TriNormal(finalVerts[finalIndices[capStartIdx+0]], finalVerts[finalIndices[capStartIdx+1]], finalVerts[finalIndices[capStartIdx+2]]);
                Vector3f capDir(-dir[0].x, -dir[0].y, 0.0f);
                float dot = N.x*capDir.x + N.y*capDir.y + N.z*capDir.z;
                if(dot < 0.0f)
                {
                    // flip both triangles
                    std::swap(finalIndices[capStartIdx+1], finalIndices[capStartIdx+2]);
                    std::swap(finalIndices[capStartIdx+4], finalIndices[capStartIdx+5]);
                }
            }

            // end cap (use vertices at i=m-1)
            size_t li = m-1;
            uint32_t eb0 = vertIndex(li,0), eb1 = vertIndex(li,1), et1 = vertIndex(li,2), et0 = vertIndex(li,3);
            size_t capEndIdx = finalIndices.size();
            finalIndices.push_back(et0); finalIndices.push_back(et1); finalIndices.push_back(eb1);
            finalIndices.push_back(et0); finalIndices.push_back(eb1); finalIndices.push_back(eb0);

            if(!dir.empty()){
                Vector3f N = TriNormal(finalVerts[finalIndices[capEndIdx+0]], finalVerts[finalIndices[capEndIdx+1]], finalVerts[finalIndices[capEndIdx+2]]);
                Vector3f capDir(dir.back().x, dir.back().y, 0.0f);
                float dot = N.x*capDir.x + N.y*capDir.y + N.z*capDir.z;
                if(dot < 0.0f)
                {
                    std::swap(finalIndices[capEndIdx+1], finalIndices[capEndIdx+2]);
                    std::swap(finalIndices[capEndIdx+4], finalIndices[capEndIdx+5]);
                }
            }
        }

        // fix triangle winding if normal.z < 0 (flip triangle) to ensure top faces are visible
        for(size_t ti=0; ti+2<finalIndices.size(); ti+=3)
        {
            uint32_t ia = finalIndices[ti+0];
            uint32_t ib = finalIndices[ti+1];
            uint32_t ic = finalIndices[ti+2];
            const Vector3f &A = finalVerts[ia];
            const Vector3f &B = finalVerts[ib];
            const Vector3f &C = finalVerts[ic];

            Vector3f N = TriNormal(A,B,C);
            if(N.z < 0.0f)
            {
                finalIndices[ti+1] = ic;
                finalIndices[ti+2] = ib;
            }
        }

        // compute per-vertex normals by accumulating triangle normals
        std::vector<Vector3f> vertNormals(finalVerts.size(), Vector3f(0,0,0));
        for(size_t ti=0; ti+2<finalIndices.size(); ti+=3)
        {
            uint32_t ia = finalIndices[ti+0];
            uint32_t ib = finalIndices[ti+1];
            uint32_t ic = finalIndices[ti+2];
            Vector3f A = finalVerts[ia];
            Vector3f B = finalVerts[ib];
            Vector3f C = finalVerts[ic];
            Vector3f N = TriNormal(A,B,C);
            vertNormals[ia].x += N.x; vertNormals[ia].y += N.y; vertNormals[ia].z += N.z;
            vertNormals[ib].x += N.x; vertNormals[ib].y += N.y; vertNormals[ib].z += N.z;
            vertNormals[ic].x += N.x; vertNormals[ic].y += N.y; vertNormals[ic].z += N.z;
        }

        for(size_t i=0;i<vertNormals.size();++i) vertNormals[i] = Normalize3(vertNormals[i]);

        if(!pc->Init("WallsFromLines", (uint)finalVerts.size(), (uint)finalIndices.size())) return nullptr;

        VABMapFloat pos_map(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm_map(pc->GetVABMap(VAN::Normal), VF_V3F);
        VABMapFloat tan_map(pc->GetVABMap(VAN::Tangent), VF_V3F);
        VABMapFloat tex_map(pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos_map;
        float *np = nrm_map;
        float *tp = tan_map;
        float *uvp = tex_map;

        for(size_t i=0;i<finalVerts.size();++i)
        {
            const auto &v = finalVerts[i];
            if(vp){ *vp++ = v.x; *vp++ = v.y; *vp++ = v.z; }
            if(np){ *np++ = vertNormals[i].x; *np++ = vertNormals[i].y; *np++ = vertNormals[i].z; }
            if(tp){ *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
            if(uvp){ *uvp++ = finalUV[i].x; *uvp++ = finalUV[i].y; }
        }

        IBMap *ib_map = pc->GetIBMap();
        const IndexType itype = pc->GetIndexType();

        if(itype==IndexType::U16){ IBTypeMap<uint16> im(ib_map); uint16 *ip=im; for(size_t i=0;i<finalIndices.size();++i) *ip++ = (uint16)finalIndices[i]; }
        else if(itype==IndexType::U32){ IBTypeMap<uint32> im(ib_map); uint32 *ip=im; for(size_t i=0;i<finalIndices.size();++i) *ip++ = (uint32)finalIndices[i]; }
        else if(itype==IndexType::U8){ IBTypeMap<uint8> im(ib_map); uint8 *ip=im; for(size_t i=0;i<finalIndices.size();++i) *ip++ = (uint8)finalIndices[i]; }
        else return nullptr;

        Primitive *p = pc->Create();
        if(p)
        {
            float minX = finalVerts[0].x, maxX = finalVerts[0].x, minY = finalVerts[0].y, maxY = finalVerts[0].y, minZ = finalVerts[0].z, maxZ = finalVerts[0].z;
            for(const auto &fv: finalVerts){ minX = std::min(minX,fv.x); maxX = std::max(maxX,fv.x); minY = std::min(minY,fv.y); maxY = std::max(maxY,fv.y); minZ = std::min(minZ,fv.z); maxZ = std::max(maxZ,fv.z); }
            AABB aabb; aabb.SetMinMax(Vector3f(minX,minY,minZ), Vector3f(maxX,maxY,maxZ)); p->SetBoundingBox(aabb);
        }

        return p;
    }
}
