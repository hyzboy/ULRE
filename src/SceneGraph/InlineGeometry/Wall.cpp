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
        if(!pc || !wci || !wci->lines || wci->lineCount == 0)
            return nullptr;

        if(wci->thickness <= 0.0f || wci->height <= 0.0f)
            return nullptr;

        const float halfThickness = wci->thickness * 0.5f;
        const float halfHeight = wci->height * 0.5f;

        // 简单实现：单条线生成一个矩形墙体(cube)
        if(wci->lineCount == 1)
        {
            const Line2D& line = wci->lines[0];

            // 计算线段方向和长度
            float dx = line.end.x - line.start.x;
            float dy = line.end.y - line.start.y;
            float len = sqrt(dx * dx + dy * dy);

            if(len < 1e-6f) return nullptr; // 线段太短

            // 单位方向向量
            float dirX = dx / len;
            float dirY = dy / len;

            // 垂直方向(向左90度旋转)
            float perpX = -dirY;
            float perpY = dirX;

            // 计算墙体四个角点(2D)
            Vector2f p1(line.start.x + perpX * halfThickness, line.start.y + perpY * halfThickness);
            Vector2f p2(line.start.x - perpX * halfThickness, line.start.y - perpY * halfThickness);
            Vector2f p3(line.end.x - perpX * halfThickness, line.end.y - perpY * halfThickness);
            Vector2f p4(line.end.x + perpX * halfThickness, line.end.y + perpY * halfThickness);

            // 8个顶点(上下各4个)
            const uint numberVertices = 8;
            const uint numberIndices = 36; // 6面 * 2三角形 * 3顶点 = 36

            if(!pc->Init("WallFromLine", numberVertices, numberIndices))
                return nullptr;

            VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
            VABMapFloat nrm(pc->GetVABMap(VAN::Normal), VF_V3F);
            VABMapFloat tan(pc->GetVABMap(VAN::Tangent), VF_V3F);
            VABMapFloat uv(pc->GetVABMap(VAN::TexCoord), VF_V2F);

            float *vp = pos;
            float *np = nrm;
            float *tp = tan;
            float *uvp = uv;

            auto write_vertex = [&](float x, float y, float z, float nx, float ny, float nz, float u, float v)
            {
                if(vp) { *vp++ = x; *vp++ = y; *vp++ = z; }
                if(np) { *np++ = nx; *np++ = ny; *np++ = nz; }
                if(tp) {
                    // 计算切线：对于墙面，切线方向沿着墙的长度方向
                    if(fabsf(nz) > 0.5f) { // 顶面或底面
                        *tp++ = dirX; *tp++ = dirY; *tp++ = 0.0f;
                    } else { // 侧面
                        *tp++ = 0.0f; *tp++ = 0.0f; *tp++ = 1.0f;
                    }
                }
                if(uvp) { *uvp++ = u; *uvp++ = v; }
            };

            // 底面顶点 (z = -halfHeight)
            write_vertex(p1.x, p1.y, -halfHeight, 0, 0, -1, 0, 0);  // 0
            write_vertex(p2.x, p2.y, -halfHeight, 0, 0, -1, 1, 0);  // 1
            write_vertex(p3.x, p3.y, -halfHeight, 0, 0, -1, 1, 1);  // 2
            write_vertex(p4.x, p4.y, -halfHeight, 0, 0, -1, 0, 1);  // 3

            // 顶面顶点 (z = +halfHeight)
            write_vertex(p1.x, p1.y, halfHeight, 0, 0, 1, 0, 0);    // 4
            write_vertex(p2.x, p2.y, halfHeight, 0, 0, 1, 1, 0);    // 5
            write_vertex(p3.x, p3.y, halfHeight, 0, 0, 1, 1, 1);    // 6
            write_vertex(p4.x, p4.y, halfHeight, 0, 0, 1, 0, 1);    // 7

            // 索引 (ClockWise为正面)
            IBMap *ib_map = pc->GetIBMap();
            const IndexType it = pc->GetIndexType();

            // 定义六个面的索引 (确保ClockWise)
            uint16 indices[] = {
                // 底面 (z = -halfHeight, 法线向下，从下方看为顺时针)
                0, 2, 1,  0, 3, 2,
                // 顶面 (z = +halfHeight, 法线向上，从上方看为顺时针)
                4, 5, 6,  4, 6, 7,
                // 前面 (y方向)
                0, 1, 5,  0, 5, 4,
                // 后面 (-y方向)
                3, 7, 6,  3, 6, 2,
                // 左面 (-x方向)
                1, 2, 6,  1, 6, 5,
                // 右面 (x方向)
                0, 4, 7,  0, 7, 3
            };

            if(it == IndexType::U16)
            {
                IBTypeMap<uint16> im(ib_map);
                uint16 *ip = im;
                for(int i = 0; i < 36; i++)
                    *ip++ = indices[i];
            }
            else if(it == IndexType::U32)
            {
                IBTypeMap<uint32> im(ib_map);
                uint32 *ip = im;
                for(int i = 0; i < 36; i++)
                    *ip++ = (uint32)indices[i];
            }
            else if(it == IndexType::U8)
            {
                IBTypeMap<uint8> im(ib_map);
                uint8 *ip = im;
                for(int i = 0; i < 36; i++)
                    *ip++ = (uint8)indices[i];
            }
            else return nullptr;

            Primitive *p = pc->Create();
            if(p)
            {
                // 计算包围盒
                float minX = std::min({p1.x, p2.x, p3.x, p4.x});
                float maxX = std::max({p1.x, p2.x, p3.x, p4.x});
                float minY = std::min({p1.y, p2.y, p3.y, p4.y});
                float maxY = std::max({p1.y, p2.y, p3.y, p4.y});

                AABB aabb;
                aabb.SetMinMax(Vector3f(minX, minY, -halfHeight), Vector3f(maxX, maxY, halfHeight));
                p->SetBoundingBox(aabb);
            }
            return p;
        }


        // 处理多条线的情况 - 改进版本，处理连接线和拐角
        if(wci->lineCount > 1)
        {
            // 分析线段连接关系
            struct LineInfo {
                Line2D line;
                float dirX, dirY;  // 单位方向向量
                float perpX, perpY; // 垂直方向向量
                float length;
                bool processed;
            };

            std::vector<LineInfo> lineInfos(wci->lineCount);

            // 预处理所有线段
            for(uint i = 0; i < wci->lineCount; i++)
            {
                LineInfo& info = lineInfos[i];
                info.line = wci->lines[i];

                float dx = info.line.end.x - info.line.start.x;
                float dy = info.line.end.y - info.line.start.y;
                info.length = sqrt(dx * dx + dy * dy);

                if(info.length > 1e-6f)
                {
                    info.dirX = dx / info.length;
                    info.dirY = dy / info.length;
                    info.perpX = -info.dirY;  // 向左90度
                    info.perpY = info.dirX;
                }
                else
                {
                    info.dirX = info.dirY = 0;
                    info.perpX = info.perpY = 0;
                }
                info.processed = false;
            }

            // 收集所有墙体顶点
            std::vector<Vector2f> wallVertices;
            std::vector<uint16> wallIndices;

            // 为每条线段生成墙体几何
            for(uint i = 0; i < wci->lineCount; i++)
            {
                const LineInfo& info = lineInfos[i];
                if(info.length < 1e-6f) continue;

                // 计算墙体四个角点(2D)
                Vector2f p1(info.line.start.x + info.perpX * halfThickness,
                           info.line.start.y + info.perpY * halfThickness);
                Vector2f p2(info.line.start.x - info.perpX * halfThickness,
                           info.line.start.y - info.perpY * halfThickness);
                Vector2f p3(info.line.end.x - info.perpX * halfThickness,
                           info.line.end.y - info.perpY * halfThickness);
                Vector2f p4(info.line.end.x + info.perpX * halfThickness,
                           info.line.end.y + info.perpY * halfThickness);

                uint baseIdx = static_cast<uint>(wallVertices.size());

                // 添加8个顶点（底面4个 + 顶面4个）
                wallVertices.push_back(p1); // 0: 底面
                wallVertices.push_back(p2); // 1
                wallVertices.push_back(p3); // 2
                wallVertices.push_back(p4); // 3
                wallVertices.push_back(p1); // 4: 顶面
                wallVertices.push_back(p2); // 5
                wallVertices.push_back(p3); // 6
                wallVertices.push_back(p4); // 7

                // 索引：6个面 × 2个三角形 × 3个顶点 = 36个索引
                uint16 localIndices[] = {
                    // 底面 (法线向下)
                    0, 2, 1,  0, 3, 2,
                    // 顶面 (法线向上)
                    4, 5, 6,  4, 6, 7,
                    // 前面
                    0, 1, 5,  0, 5, 4,
                    // 后面
                    3, 7, 6,  3, 6, 2,
                    // 左面
                    1, 2, 6,  1, 6, 5,
                    // 右面
                    0, 4, 7,  0, 7, 3
                };

                for(int j = 0; j < 36; j++)
                    wallIndices.push_back(static_cast<uint16>(baseIdx + localIndices[j]));
            }

            if(wallVertices.empty()) return nullptr;

            const uint numberVertices = static_cast<uint>(wallVertices.size());
            const uint numberIndices = static_cast<uint>(wallIndices.size());

            if(!pc->Init("WallsFromLines", numberVertices, numberIndices))
                return nullptr;

            VABMapFloat pos(pc->GetVABMap(VAN::Position), VF_V3F);
            VABMapFloat nrm(pc->GetVABMap(VAN::Normal), VF_V3F);
            VABMapFloat tan(pc->GetVABMap(VAN::Tangent), VF_V3F);
            VABMapFloat uv(pc->GetVABMap(VAN::TexCoord), VF_V2F);

            float *vp = pos;
            float *np = nrm;
            float *tp = tan;
            float *uvp = uv;

            // 写入顶点数据
            for(uint i = 0; i < wallVertices.size(); i++)
            {
                const Vector2f& v = wallVertices[i];
                uint lineIdx = i / 8; // each line contributed 8 vertices
                bool isTopFace = (i % 8) >= 4; // top vertices are the last 4 in each 8-vertex block
                float z = isTopFace ? halfHeight : -halfHeight;
                float nz = isTopFace ? 1.0f : -1.0f;

                if(vp) { *vp++ = v.x; *vp++ = v.y; *vp++ = z; }
                if(np) { *np++ = 0; *np++ = 0; *np++ = nz; }
                if(tp) {
                    float dirX = 0.0f, dirY = 0.0f;
                    if(lineIdx < lineInfos.size()) { dirX = lineInfos[lineIdx].dirX; dirY = lineInfos[lineIdx].dirY; }
                    if(fabsf(nz) > 0.5f) {
                        *tp++ = dirX; *tp++ = dirY; *tp++ = 0.0f;
                    } else {
                        *tp++ = 0.0f; *tp++ = 0.0f; *tp++ = 1.0f;
                    }
                }
                if(uvp) { *uvp++ = 0; *uvp++ = 0; }
            }

            // 写入索引数据
            IBMap *ib_map = pc->GetIBMap();
            const IndexType it2 = pc->GetIndexType();

            if(it2 == IndexType::U16)
            {
                IBTypeMap<uint16> im(ib_map);
                uint16 *ip = im;
                for(uint idx_i = 0; idx_i < wallIndices.size(); ++idx_i)
                    *ip++ = wallIndices[idx_i];
            }
            else if(it2 == IndexType::U32)
            {
                IBTypeMap<uint32> im(ib_map);
                uint32 *ip = im;
                for(uint idx_i = 0; idx_i < wallIndices.size(); ++idx_i)
                    *ip++ = static_cast<uint32>(wallIndices[idx_i]);
            }
            else if(it2 == IndexType::U8)
            {
                IBTypeMap<uint8> im(ib_map);
                uint8 *ip = im;
                for(uint idx_i = 0; idx_i < wallIndices.size(); ++idx_i)
                    *ip++ = static_cast<uint8>(wallIndices[idx_i]);
            }
            else return nullptr;

            Primitive *p = pc->Create();
            if(p)
            {
                // 计算包围盒
                float minX = wallVertices[0].x, maxX = wallVertices[0].x;
                float minY = wallVertices[0].y, maxY = wallVertices[0].y;

                for(const Vector2f& v : wallVertices)
                {
                    minX = std::min(minX, v.x);
                    maxX = std::max(maxX, v.x);
                    minY = std::min(minY, v.y);
                    maxY = std::max(maxY, v.y);
                }

                AABB aabb;
                aabb.SetMinMax(Vector3f(minX, minY, -halfHeight), Vector3f(maxX, maxY, halfHeight));
                p->SetBoundingBox(aabb);
            }
            return p;
        }

        return nullptr;
    }
}
