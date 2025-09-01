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
    Primitive *CreateWallsFromLines2D(PrimitiveCreater *pc, const WallCreateInfo *wci)
    {
        if(!pc || !wci)
            return nullptr;

        // Need vertices + either indices or lines (indices in Line2D)
        const Vector2f* verts = wci->vertices;
        uint vcount = wci->vertexCount;

        std::vector<Line2D> inputLines;

        if(wci->indices && wci->indexCount >= 2 && verts && vcount > 0)
        {
            // indices are pairs
            for(uint i=0;i+1<wci->indexCount;i+=2)
            {
                uint a = wci->indices[i];
                uint b = wci->indices[i+1];
                if(a < vcount && b < vcount) inputLines.emplace_back(a,b);
            }
        }
        else if(wci->lines && wci->lineCount > 0 && verts && vcount > 0)
        {
            // lines are index pairs
            for(uint i=0;i<wci->lineCount;i++)
            {
                const Line2D &l = wci->lines[i];
                if(l.a < vcount && l.b < vcount) inputLines.push_back(l);
            }
        }
        else
        {
            return nullptr; // insufficient data
        }

        if(inputLines.empty()) return nullptr;

        const float halfThickness = wci->thickness * 0.5f;
        const float halfHeight = wci->height * 0.5f;

        // single line fast path
        if(inputLines.size()==1)
        {
            const auto &li = inputLines[0];
            Vector2f a = verts[li.a];
            Vector2f b = verts[li.b];

            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float len = sqrt(dx*dx+dy*dy);
            if(len < 1e-6f) return nullptr;

            float dirX = dx/len, dirY = dy/len;
            float perpX = -dirY, perpY = dirX;

            Vector2f p1(a.x + perpX*halfThickness, a.y + perpY*halfThickness);
            Vector2f p2(a.x - perpX*halfThickness, a.y - perpY*halfThickness);
            Vector2f p3(b.x - perpX*halfThickness, b.y - perpY*halfThickness);
            Vector2f p4(b.x + perpX*halfThickness, b.y + perpY*halfThickness);

            const uint numberVertices = 8;
            const uint numberIndices = 36;

            if(!pc->Init("WallFromLine", numberVertices, numberIndices)) return nullptr;

            VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
            VABMapFloat nrm(pc->GetVABMap(VAN::Normal), VF_V3F);
            VABMapFloat tan(pc->GetVABMap(VAN::Tangent), VF_V3F);
            VABMapFloat uv(pc->GetVABMap(VAN::TexCoord), VF_V2F);

            float *vp = pos;
            float *np = nrm;
            float *tp = tan;
            float *uvp = uv;

            auto write_vertex=[&](const Vector2f &p, float z, float nz, float u, float v)
            {
                if(vp) { *vp++ = p.x; *vp++ = p.y; *vp++ = z; }
                if(np) { *np++ = 0.0f; *np++ = 0.0f; *np++ = nz; }
                if(tp) {
                    if(fabsf(nz)>0.5f){ *tp++ = dirX; *tp++ = dirY; *tp++ = 0.0f; }
                    else { *tp++ = 0.0f; *tp++ = 0.0f; *tp++ = 1.0f; }
                }
                if(uvp) { *uvp++ = u; *uvp++ = v; }
            };

            write_vertex(p1,-halfHeight,-1.0f,0,0);
            write_vertex(p2,-halfHeight,-1.0f,1,0);
            write_vertex(p3,-halfHeight,-1.0f,1,1);
            write_vertex(p4,-halfHeight,-1.0f,0,1);

            write_vertex(p1,halfHeight,1.0f,0,0);
            write_vertex(p2,halfHeight,1.0f,1,0);
            write_vertex(p3,halfHeight,1.0f,1,1);
            write_vertex(p4,halfHeight,1.0f,0,1);

            IBMap *ib_map = pc->GetIBMap();
            const IndexType it = pc->GetIndexType();

            uint16 indices[] = {0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,1,5, 0,5,4, 3,7,6, 3,6,2, 1,2,6, 1,6,5, 0,4,7, 0,7,3};

            if(it==IndexType::U16){ IBTypeMap<uint16> im(ib_map); uint16 *ip=im; for(int i=0;i<36;i++) *ip++=indices[i]; }
            else if(it==IndexType::U32){ IBTypeMap<uint32> im(ib_map); uint32 *ip=im; for(int i=0;i<36;i++) *ip++=(uint32)indices[i]; }
            else if(it==IndexType::U8){ IBTypeMap<uint8> im(ib_map); uint8 *ip=im; for(int i=0;i<36;i++) *ip++=(uint8)indices[i]; }
            else return nullptr;

            Primitive *p = pc->Create();
            if(p){ AABB aabb; aabb.SetMinMax(Vector3f(std::min({p1.x,p2.x,p3.x,p4.x}), std::min({p1.y,p2.y,p3.y,p4.y}), -halfHeight),
                                           Vector3f(std::max({p1.x,p2.x,p3.x,p4.x}), std::max({p1.y,p2.y,p3.y,p4.y}), halfHeight)); p->SetBoundingBox(aabb); }
            return p;
        }

        // 多条线：为每条线生成8个顶点（底4 顶4），后续可以在这里合并顶点以处理相交
        std::vector<Vector2f> vert2d;
        std::vector<uint32_t> indices;
        vert2d.reserve(inputLines.size()*8);
        indices.reserve(inputLines.size()*36);

        for(size_t i=0;i<inputLines.size();++i)
        {
            const Line2D &l = inputLines[i];
            Vector2f a = verts[l.a];
            Vector2f b = verts[l.b];

            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float len = sqrt(dx*dx+dy*dy);
            if(len < 1e-6f) continue;

            float dirX = dx/len, dirY = dy/len;
            float perpX = -dirY, perpY = dirX;

            Vector2f p1(a.x + perpX*halfThickness, a.y + perpY*halfThickness);
            Vector2f p2(a.x - perpX*halfThickness, a.y - perpY*halfThickness);
            Vector2f p3(b.x - perpX*halfThickness, b.y - perpY*halfThickness);
            Vector2f p4(b.x + perpX*halfThickness, b.y + perpY*halfThickness);

            uint32_t base = static_cast<uint32_t>(vert2d.size());

            vert2d.push_back(p1);
            vert2d.push_back(p2);
            vert2d.push_back(p3);
            vert2d.push_back(p4);

            // top
            vert2d.push_back(p1);
            vert2d.push_back(p2);
            vert2d.push_back(p3);
            vert2d.push_back(p4);

            uint16 localIdx[36] = {0,2,1, 0,3,2, 4,5,6, 4,6,7, 0,1,5, 0,5,4, 3,7,6, 3,6,2, 1,2,6, 1,6,5, 0,4,7, 0,7,3};

            for(int k=0;k<36;k++) indices.push_back(base + localIdx[k]);
        }

        if(vert2d.empty()) return nullptr;

        if(!pc->Init("WallsFromLines", static_cast<uint>(vert2d.size()), static_cast<uint>(indices.size()))) return nullptr;

        VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat nrm(pc->GetVABMap(VAN::Normal), VF_V3F);
        VABMapFloat tan(pc->GetVABMap(VAN::Tangent), VF_V3F);
        VABMapFloat uv(pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = pos;
        float *np = nrm;
        float *tp = tan;
        float *uvp = uv;

        for(size_t i=0;i<vert2d.size();++i)
        {
            const Vector2f &v = vert2d[i];
            bool isTop = (i%8)>=4;
            float z = isTop?halfHeight:-halfHeight;
            float nz = isTop?1.0f:-1.0f;

            if(vp){ *vp++ = v.x; *vp++ = v.y; *vp++ = z; }
            if(np){ *np++ = 0.0f; *np++ = 0.0f; *np++ = nz; }
            if(tp){ *tp++ = 0.0f; *tp++ = 0.0f; *tp++ = 1.0f; }
            if(uvp){ *uvp++ = 0.0f; *uvp++ = 0.0f; }
        }

        IBMap *ib_map = pc->GetIBMap();
        const IndexType it2 = pc->GetIndexType();

        if(it2==IndexType::U16){ IBTypeMap<uint16> im(ib_map); uint16 *ip=im; for(size_t i=0;i<indices.size();++i) *ip++=(uint16)indices[i]; }
        else if(it2==IndexType::U32){ IBTypeMap<uint32> im(ib_map); uint32 *ip=im; for(size_t i=0;i<indices.size();++i) *ip++=(uint32)indices[i]; }
        else if(it2==IndexType::U8){ IBTypeMap<uint8> im(ib_map); uint8 *ip=im; for(size_t i=0;i<indices.size();++i) *ip++=(uint8)indices[i]; }
        else return nullptr;

        Primitive *p = pc->Create();
        if(p)
        {
            float minX = vert2d[0].x, maxX = vert2d[0].x, minY = vert2d[0].y, maxY = vert2d[0].y;
            for(const auto &vv:vert2d){ minX = std::min(minX,vv.x); maxX = std::max(maxX,vv.x); minY = std::min(minY,vv.y); maxY = std::max(maxY,vv.y); }
            AABB aabb; aabb.SetMinMax(Vector3f(minX,minY,-halfHeight), Vector3f(maxX,maxY,halfHeight)); p->SetBoundingBox(aabb);
        }

        return p;
    }
}
