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
    static bool LineLineIntersect(const Vector2f &p, const Vector2f &r, const Vector2f &q, const Vector2f &s, float &t, float &u)
    {
        float rxs = r.x * s.y - r.y * s.x;
        if (fabs(rxs) < 1e-8f) return false;
        Vector2f qp = Vector2f(q.x - p.x, q.y - p.y);
        t = (qp.x * s.y - qp.y * s.x) / rxs;
        u = (qp.x * r.y - qp.y * r.x) / rxs;
        return true;
    }

    static Vector2f Normalize2(const Vector2f &v)
    {
        float len = sqrt(v.x*v.x + v.y*v.y);
        if(len <= 1e-8f) return Vector2f(0,0);
        return Vector2f(v.x/len, v.y/len);
    }

    static Vector3f TriNormal(const Vector3f &A,const Vector3f &B,const Vector3f &C)
    {
        Vector3f AB(B.x-A.x, B.y-A.y, B.z-A.z);
        Vector3f AC(C.x-A.x, C.y-A.y, C.z-A.z);
        return Vector3f( AB.y*AC.z - AB.z*AC.y,
                         AB.z*AC.x - AB.x*AC.z,
                         AB.x*AC.y - AB.y*AC.x );
    }

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

        std::vector<uint> seq; seq.reserve(segA.size()+1);
        std::vector<char> visited(vcount,0);
        int curr = start; int prev = -1;
        while(curr!=-1)
        {
            seq.push_back((uint)curr);
            visited[curr]=1;
            int next=-1;
            for(uint nb: adj[curr]){ if((int)nb==prev) continue; next=(int)nb; break; }
            prev=curr; curr=next; if(curr!=-1 && visited[curr]) break;
        }
        bool closed = false;
        if(!seq.empty()){ uint first=seq.front(), last=seq.back(); for(uint nb: adj[last]) if(nb==first){ closed=true; break; } }

        const float halfT = wci->thickness * 0.5f;
        const float halfH = wci->height * 0.5f;

        size_t nverts = seq.size(); if(nverts<2) return nullptr;

        // build per-segment data
        struct Seg{ Vector2f p0,p1; Vector2f dir; Vector2f nrm; float len; Vector2f left0,right0,left1,right1; };
        std::vector<Seg> segs;
        segs.reserve(nverts);
        if(closed)
        {
            // include closing segment for closed polyline
            for(size_t i=0;i<nverts;++i)
            {
                size_t ni = (i+1)%nverts;
                Seg s;
                s.p0 = verts[ seq[i] ]; s.p1 = verts[ seq[ni] ];
                Vector2f d(s.p1.x - s.p0.x, s.p1.y - s.p0.y);
                float L = sqrt(d.x*d.x + d.y*d.y);
                if(L<=1e-8f){ s.dir = Vector2f(0,0); s.nrm = Vector2f(0,0); s.len = 0; }
                else{ s.dir = Vector2f(d.x/L, d.y/L); s.nrm = Vector2f(-s.dir.y, s.dir.x); s.len = L; }
                s.left0 = Vector2f(s.p0.x + s.nrm.x*halfT, s.p0.y + s.nrm.y*halfT);
                s.right0 = Vector2f(s.p0.x - s.nrm.x*halfT, s.p0.y - s.nrm.y*halfT);
                s.left1 = Vector2f(s.p1.x + s.nrm.x*halfT, s.p1.y + s.nrm.y*halfT);
                s.right1 = Vector2f(s.p1.x - s.nrm.x*halfT, s.p1.y - s.nrm.y*halfT);
                s.len = L;
                segs.push_back(s);
            }
        }
        else
        {
            for(size_t i=0;i+1<nverts;++i)
            {
                Seg s;
                s.p0 = verts[ seq[i] ]; s.p1 = verts[ seq[i+1] ];
                Vector2f d(s.p1.x - s.p0.x, s.p1.y - s.p0.y);
                float L = sqrt(d.x*d.x + d.y*d.y);
                if(L<=1e-8f){ s.dir = Vector2f(0,0); s.nrm = Vector2f(0,0); s.len = 0; }
                else{ s.dir = Vector2f(d.x/L, d.y/L); s.nrm = Vector2f(-s.dir.y, s.dir.x); s.len = L; }
                s.left0 = Vector2f(s.p0.x + s.nrm.x*halfT, s.p0.y + s.nrm.y*halfT);
                s.right0 = Vector2f(s.p0.x - s.nrm.x*halfT, s.p0.y - s.nrm.y*halfT);
                s.left1 = Vector2f(s.p1.x + s.nrm.x*halfT, s.p1.y + s.nrm.y*halfT);
                s.right1 = Vector2f(s.p1.x - s.nrm.x*halfT, s.p1.y - s.nrm.y*halfT);
                s.len = L;
                segs.push_back(s);
            }
        }

        // per-vertex join handling: for each vertex vi, compute leftPoints and rightPoints arrays
        struct VJoin{ std::vector<Vector2f> leftPts; std::vector<Vector2f> rightPts; };
        std::vector<VJoin> vjoin(nverts);

        for(size_t vi=0;vi<nverts;++vi)
        {
            bool hasPrev = (vi>0);
            bool hasNext = (vi+1<nverts);
            if(closed){ hasPrev=true; hasNext=true; }

            if(!hasPrev && hasNext)
            {
                // start vertex: use segment 0 start offsets
                vjoin[vi].leftPts.push_back(segs[0].left0);
                vjoin[vi].rightPts.push_back(segs[0].right0);
                continue;
            }
            if(!hasNext && hasPrev)
            {
                // end vertex: use last segment end offsets
                vjoin[vi].leftPts.push_back(segs[vi-1].left1);
                vjoin[vi].rightPts.push_back(segs[vi-1].right1);
                continue;
            }

            // interior vertex: between seg (vi-1) and seg (vi)
            size_t prevSegIndex = (vi==0)? (segs.size()-1) : (vi-1);
            size_t nextSegIndex = vi % segs.size();
            Seg &sPrev = segs[prevSegIndex];
            Seg &sNext = segs[nextSegIndex];

            // left side join
            bool left_ok = false;
            Vector2f left_pt;
            {
                float t,u;
                if(sPrev.len>1e-8f && sNext.len>1e-8f)
                {
                    if(LineLineIntersect(sPrev.left1, sPrev.dir, sNext.left0, sNext.dir, t, u))
                    {
                        left_pt = Vector2f(sPrev.left1.x + sPrev.dir.x * t, sPrev.left1.y + sPrev.dir.y * t);
                        float miter_len = sqrt((left_pt.x - verts[seq[vi]].x)*(left_pt.x - verts[seq[vi]].x) + (left_pt.y - verts[seq[vi]].y)*(left_pt.y - verts[seq[vi]].y));
                        if(!(miter_len > wci->miterLimit * halfT && wci->cornerJoin == WallCreateInfo::CornerJoin::Miter)) left_ok = true;
                    }
                }
            }

            // right side join
            bool right_ok = false;
            Vector2f right_pt;
            {
                float t,u;
                if(sPrev.len>1e-8f && sNext.len>1e-8f)
                {
                    if(LineLineIntersect(sPrev.right1, sPrev.dir, sNext.right0, sNext.dir, t, u))
                    {
                        right_pt = Vector2f(sPrev.right1.x + sPrev.dir.x * t, sPrev.right1.y + sPrev.dir.y * t);
                        float miter_len = sqrt((right_pt.x - verts[seq[vi]].x)*(right_pt.x - verts[seq[vi]].x) + (right_pt.y - verts[seq[vi]].y)*(right_pt.y - verts[seq[vi]].y));
                        if(!(miter_len > wci->miterLimit * halfT && wci->cornerJoin == WallCreateInfo::CornerJoin::Miter)) right_ok = true;
                    }
                }
            }

            if(wci->cornerJoin == WallCreateInfo::CornerJoin::Miter && left_ok && right_ok)
            {
                vjoin[vi].leftPts.push_back(left_pt);
                vjoin[vi].rightPts.push_back(right_pt);
                // also update adjacent segment endpoints to use these miter points
                sPrev.left1 = left_pt; sNext.left0 = left_pt;
                sPrev.right1 = right_pt; sNext.right0 = right_pt;
            }
            else if(wci->cornerJoin == WallCreateInfo::CornerJoin::Bevel)
            {
                // Bevel: 钝角侧为Beval（两个顶点），锐角侧只保留一个顶点（中点）
                float cross = sPrev.dir.x * sNext.dir.y - sPrev.dir.y * sNext.dir.x;
                if(cross < 0.0f)
                {
                    // 左侧为钝角，右侧为锐角
                    // left为Bevel（两个顶点）
                    vjoin[vi].leftPts.push_back(sPrev.left1);
                    vjoin[vi].leftPts.push_back(sNext.left0);
                    // right只保留一个顶点（中点）
                    Vector2f right_mid = Vector2f((sPrev.right1.x + sNext.right0.x)*0.5f, (sPrev.right1.y + sNext.right0.y)*0.5f);
                    vjoin[vi].rightPts.push_back(right_mid);
                }
                else
                {
                    // 右侧为钝角，左侧为锐角
                    // right为Beval（两个顶点）
                    vjoin[vi].rightPts.push_back(sPrev.right1);
                    vjoin[vi].rightPts.push_back(sNext.right0);
                    // left只保留一个顶点（中点）
                    Vector2f left_mid = Vector2f((sPrev.left1.x + sNext.left0.x)*0.5f, (sPrev.left1.y + sNext.left0.y)*0.5f);
                    vjoin[vi].leftPts.push_back(left_mid);
                }
            }
            else // Round
            {
                // generate arc only on outer side, inner side single vertex -> fan
                float a1_left = atan2(sPrev.nrm.y, sPrev.nrm.x);
                float a2_left = atan2(sNext.nrm.y, sNext.nrm.x);
                float da_left = a2_left - a1_left; while(da_left <= -HGL_PI) da_left += 2*HGL_PI; while(da_left > HGL_PI) da_left -= 2*HGL_PI;

                int segs = std::max<int>(3, (int)wci->roundSegments);

                // determine turn direction: cross(dPrev, dNext)
                float cross = sPrev.dir.x * sNext.dir.y - sPrev.dir.y * sNext.dir.x;

                // NOTE: treat cross < 0 as left outer to match coordinate winding (fix reversed acute/obtuse side)
                if(cross < 0.0f)
                {
                    // left is outer: generate left arc, keep right as single center
                    for(int s=0;s<=segs;s++)
                    {
                        float tseg = (float)s/(float)segs;
                        float ang = a1_left + da_left * tseg;
                        vjoin[vi].leftPts.push_back(Vector2f(verts[seq[vi]].x + cos(ang)*halfT, verts[seq[vi]].y + sin(ang)*halfT));
                    }

                    // single inner point: average of adjacent right offsets to avoid gaps
                    Vector2f right_single = Vector2f((sPrev.right1.x + sNext.right0.x)*0.5f, (sPrev.right1.y + sNext.right0.y)*0.5f);
                    vjoin[vi].rightPts.push_back(right_single);

                    // attach arc ends to adjacent segments
                    if(!vjoin[vi].leftPts.empty()){ sPrev.left1 = vjoin[vi].leftPts.front(); sNext.left0 = vjoin[vi].leftPts.back(); }
                    sPrev.right1 = right_single; sNext.right0 = right_single;
                }
                else
                {
                    // right is outer: generate right arc, keep left as single center
                    float a1_right = a1_left + (float)M_PI;
                    float a2_right = a2_left + (float)M_PI;
                    float da_right = a2_right - a1_right; while(da_right <= -HGL_PI) da_right += 2*HGL_PI; while(da_right > HGL_PI) da_right -= 2*HGL_PI;

                    for(int s=0;s<=segs;s++)
                    {
                        float tseg = (float)s/(float)segs;
                        float ang = a1_right + da_right * tseg;
                        vjoin[vi].rightPts.push_back(Vector2f(verts[seq[vi]].x + cos(ang)*halfT, verts[seq[vi]].y + sin(ang)*halfT));
                    }

                    Vector2f left_single = Vector2f((sPrev.left1.x + sNext.left0.x)*0.5f, (sPrev.left1.y + sNext.left0.y)*0.5f);
                    vjoin[vi].leftPts.push_back(left_single);

                    if(!vjoin[vi].rightPts.empty()){ sPrev.right1 = vjoin[vi].rightPts.front(); sNext.right0 = vjoin[vi].rightPts.back(); }
                    sPrev.left1 = left_single; sNext.left0 = left_single;
                }
            }
        }

        // Build left_poly and right_poly by concatenating per-vertex point lists in order
        std::vector<Vector2f> left_poly, right_poly;
        for(size_t vi=0; vi<nverts; ++vi)
        {
            // Ensure left/right counts match for this vertex
            size_t lcount = vjoin[vi].leftPts.size();
            size_t rcount = vjoin[vi].rightPts.size();
            if(lcount==0 && rcount==0) continue;
            if(lcount==0) { vjoin[vi].leftPts.push_back(vjoin[vi].rightPts.front()); lcount=1; }
            if(rcount==0) { vjoin[vi].rightPts.push_back(vjoin[vi].leftPts.front()); rcount=1; }

            if(lcount == rcount)
            {
                for(size_t k=0;k<lcount;k++){ left_poly.push_back(vjoin[vi].leftPts[k]); right_poly.push_back(vjoin[vi].rightPts[k]); }
            }
            else if(lcount == 1 && rcount > 1)
            {
                // duplicate left point to match
                for(size_t k=0;k<rcount;k++){ left_poly.push_back(vjoin[vi].leftPts[0]); right_poly.push_back(vjoin[vi].rightPts[k]); }
            }
            else if(rcount == 1 && lcount > 1)
            {
                for(size_t k=0;k<lcount;k++){ left_poly.push_back(vjoin[vi].leftPts[k]); right_poly.push_back(vjoin[vi].rightPts[0]); }
            }
            else
            {
                // fallback: sample min count
                size_t cnt = std::min(lcount, rcount);
                for(size_t k=0;k<cnt;k++){ left_poly.push_back(vjoin[vi].leftPts[k]); right_poly.push_back(vjoin[vi].rightPts[k]); }
            }
        }

        // now we have left_poly and right_poly with equal length
        if(left_poly.size() != right_poly.size()) return nullptr;
        size_t m = left_poly.size(); if(m < 2) return nullptr;

        // build final 3D vertices and UVs
        std::vector<Vector3f> finalVerts; finalVerts.reserve(m*6);
        std::vector<Vector2f> finalUV; finalUV.reserve(m*6);

        // precompute accum along base polyline for U mapping (approximate by nearest original vertex)
        std::vector<float> accum(nverts); accum[0]=0.0f; for(size_t i=1;i<nverts;i++) accum[i]=accum[i-1] + segs[i-1].len;

        for(size_t i=0;i<m;i++)
        {
            Vector2f L = left_poly[i]; Vector2f R = right_poly[i];
            // Side vertices (used for vertical faces) - separate from cap verts
            finalVerts.push_back(Vector3f(L.x, L.y, -halfH)); // 0 side bottom left
            finalVerts.push_back(Vector3f(R.x, R.y, -halfH)); // 1 side bottom right
            finalVerts.push_back(Vector3f(R.x, R.y, halfH));  // 2 side top right
            finalVerts.push_back(Vector3f(L.x, L.y, halfH));  // 3 side top left

            // Cap (top) vertices - separate instances so normals/uv differ from sides
            finalVerts.push_back(Vector3f(L.x, L.y, halfH));  // 4 top cap left
            finalVerts.push_back(Vector3f(R.x, R.y, halfH));  // 5 top cap right

            // approximate u by nearest original vertex index
            float minDist = FLT_MAX; size_t bestIdx=0;
            for(size_t vi=0; vi<nverts; ++vi)
            {
                Vector2f op = verts[ seq[vi] ];
                float dx = L.x - op.x, dy = L.y - op.y; float d = dx*dx + dy*dy;
                if(d<minDist){ minDist=d; bestIdx=vi; }
            }
            float u = accum[bestIdx] * wci->uv_u_repeat_per_unit;
            // UVs for side vertices: bottom/ top share same u. v for side uses 0..uv_tile_v
            float v0 = 0.0f; float v1 = wci->uv_tile_v;
            finalUV.push_back(Vector2f(u,v0)); finalUV.push_back(Vector2f(u,v0)); finalUV.push_back(Vector2f(u,v1)); finalUV.push_back(Vector2f(u,v1));

            // UVs for cap vertices: use u same, v across width (left->0, right->1)
            finalUV.push_back(Vector2f(u,0.0f)); finalUV.push_back(Vector2f(u,1.0f));
        }

        // indices
        std::vector<uint32_t> finalIndices; finalIndices.reserve((m-(closed?0:1))*18 + 12);
        auto vertIndex = [&](size_t i,int corner)->uint32_t{ return (uint32_t)(i*6 + corner); };
        size_t segCount = closed? m : (m-1);

        // decide if vertical faces need per-face independent vertices
        bool flatSide = (wci->cornerJoin == WallCreateInfo::CornerJoin::Miter || wci->cornerJoin == WallCreateInfo::CornerJoin::Bevel);

        for(size_t i=0;i<segCount;i++)
        {
            size_t ni = (i+1)%m;
            // Top (horizontal) face -> use cap vertices (4,5) to have independent normals/uvs
            uint32_t t0 = vertIndex(i,4), t1 = vertIndex(ni,4), t2 = vertIndex(ni,5), t3 = vertIndex(i,5);
            finalIndices.push_back(t0); finalIndices.push_back(t2); finalIndices.push_back(t1);
            finalIndices.push_back(t0); finalIndices.push_back(t3); finalIndices.push_back(t2);

            // Left vertical face
            uint32_t orig_l0 = vertIndex(i,0), orig_l1 = vertIndex(i,3), orig_l2 = vertIndex(ni,3), orig_l3 = vertIndex(ni,0);
            if(flatSide)
            {
                uint32_t base = (uint32_t)finalVerts.size();
                // duplicate positions and uvs for a unique quad
                finalVerts.push_back(finalVerts[orig_l0]); finalVerts.push_back(finalVerts[orig_l1]); finalVerts.push_back(finalVerts[orig_l2]); finalVerts.push_back(finalVerts[orig_l3]);
                finalUV.push_back(finalUV[orig_l0]); finalUV.push_back(finalUV[orig_l1]); finalUV.push_back(finalUV[orig_l2]); finalUV.push_back(finalUV[orig_l3]);
                finalIndices.push_back(base+0); finalIndices.push_back(base+1); finalIndices.push_back(base+2);
                finalIndices.push_back(base+0); finalIndices.push_back(base+2); finalIndices.push_back(base+3);
            }
            else
            {
                uint32_t l0 = orig_l0, l1 = orig_l1, l2 = orig_l2, l3 = orig_l3;
                finalIndices.push_back(l0); finalIndices.push_back(l1); finalIndices.push_back(l2);
                finalIndices.push_back(l0); finalIndices.push_back(l2); finalIndices.push_back(l3);
            }

            // Right vertical face
            uint32_t orig_r0 = vertIndex(i,1), orig_r1 = vertIndex(ni,1), orig_r2 = vertIndex(ni,2), orig_r3 = vertIndex(i,2);
            if(flatSide)
            {
                uint32_t base = (uint32_t)finalVerts.size();
                finalVerts.push_back(finalVerts[orig_r0]); finalVerts.push_back(finalVerts[orig_r1]); finalVerts.push_back(finalVerts[orig_r2]); finalVerts.push_back(finalVerts[orig_r3]);
                finalUV.push_back(finalUV[orig_r0]); finalUV.push_back(finalUV[orig_r1]); finalUV.push_back(finalUV[orig_r2]); finalUV.push_back(finalUV[orig_r3]);
                finalIndices.push_back(base+0); finalIndices.push_back(base+1); finalIndices.push_back(base+2);
                finalIndices.push_back(base+0); finalIndices.push_back(base+2); finalIndices.push_back(base+3);
            }
            else
            {
                uint32_t r0 = orig_r0, r1 = orig_r1, r2 = orig_r2, r3 = orig_r3;
                finalIndices.push_back(r0); finalIndices.push_back(r1); finalIndices.push_back(r2);
                finalIndices.push_back(r0); finalIndices.push_back(r2); finalIndices.push_back(r3);
            }
        }

        // caps for open polyline
        if(!closed)
        {
            // start end - only top cap (no bottom)
            uint32_t base0 = vertIndex(0,0);
            uint32_t capL0 = vertIndex(0,4), capR0 = vertIndex(0,5), sR0 = vertIndex(0,2), sL0 = vertIndex(0,3);
            finalIndices.push_back(capL0); finalIndices.push_back(capR0); finalIndices.push_back(sR0);
            finalIndices.push_back(capL0); finalIndices.push_back(sR0); finalIndices.push_back(sL0);

            // end end - only top cap
            size_t li = m-1; uint32_t ebase = vertIndex(li,0);
            uint32_t capL1 = vertIndex(li,4), capR1 = vertIndex(li,5), sR1 = vertIndex(li,2), sL1 = vertIndex(li,3);
            finalIndices.push_back(capL1); finalIndices.push_back(capR1); finalIndices.push_back(sR1);
            finalIndices.push_back(capL1); finalIndices.push_back(sR1); finalIndices.push_back(sL1);
        }

        // fix winding
        for(size_t ti=0; ti+2<finalIndices.size(); ti+=3)
        {
            uint32_t ia=finalIndices[ti+0], ib=finalIndices[ti+1], ic=finalIndices[ti+2];
            Vector3f A=finalVerts[ia], B=finalVerts[ib], C=finalVerts[ic];
            Vector3f N = TriNormal(A,B,C);
            if(N.z < 0.0f) { finalIndices[ti+1]=ic; finalIndices[ti+2]=ib; }
        }

        // compute normals
        std::vector<Vector3f> vertNormals(finalVerts.size(), Vector3f(0,0,0));
        for(size_t ti=0; ti+2<finalIndices.size(); ti+=3)
        {
            uint32_t ia=finalIndices[ti+0], ib=finalIndices[ti+1], ic=finalIndices[ti+2];
            Vector3f N = TriNormal(finalVerts[ia], finalVerts[ib], finalVerts[ic]);
            vertNormals[ia].x += N.x; vertNormals[ia].y += N.y; vertNormals[ia].z += N.z;
            vertNormals[ib].x += N.x; vertNormals[ib].y += N.y; vertNormals[ib].z += N.z;
            vertNormals[ic].x += N.x; vertNormals[ic].y += N.y; vertNormals[ic].z += N.z;
        }
        for(size_t i=0;i<vertNormals.size();++i) vertNormals[i]=Normalize3(vertNormals[i]);

        // ensure cap (top) vertices have perfect upward normal
        for(size_t i=0;i<m;i++)
        {
            size_t capL = i*6 + 4; size_t capR = i*6 + 5;
            if(capL < vertNormals.size()){ vertNormals[capL] = Vector3f(0,0,1); }
            if(capR < vertNormals.size()){ vertNormals[capR] = Vector3f(0,0,1); }
        }

        if(!pc->Init("WallsFromLines", (uint)finalVerts.size(), (uint)finalIndices.size())) return nullptr;

        VABMapFloat pos_map(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm_map(pc->GetVABMap(VAN::Normal), VF_V3F);
        VABMapFloat tan_map(pc->GetVABMap(VAN::Tangent), VF_V3F);
        VABMapFloat tex_map(pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos_map; float *np = nrm_map; float *tp = tan_map; float *uvp = tex_map;
        for(size_t i=0;i<finalVerts.size();++i)
        {
            const auto &v = finalVerts[i]; if(vp){ *vp++=v.x; *vp++=v.y; *vp++=v.z; }
            if(np){ *np++ = vertNormals[i].x; *np++ = vertNormals[i].y; *np++ = vertNormals[i].z; }
            if(tp){ *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
            if(uvp){ *uvp++ = finalUV[i].x; *uvp++ = finalUV[i].y; }
        }

        IBMap *ib_map = pc->GetIBMap(); const IndexType itype = pc->GetIndexType();
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
