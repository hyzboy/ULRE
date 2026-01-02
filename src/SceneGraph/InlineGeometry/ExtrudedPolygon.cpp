// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/geo/Extruded.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    // 辅助函数：构建局部坐标系
    void BuildLocalCoordinateSystem(const Vector3f& extrudeAxis, Vector3f& right, Vector3f& up)
    {
        Vector3f axis = glm::normalize(extrudeAxis);

        // 为了使结果更直观，对于常见的轴向使用固定的局部坐标系
        if (fabs(axis.x) > 0.99f) {
            // X轴挤压：使用Y作为right，Z作为up
            right = (axis.x > 0) ? Vector3f(0, 1, 0) : Vector3f(0, -1, 0);
            up = Vector3f(0, 0, 1);
        } else if (fabs(axis.y) > 0.99f) {
            // Y轴挤压：使用Z作为right，X作为up
            right = (axis.y > 0) ? Vector3f(0, 0, 1) : Vector3f(0, 0, -1);
            up = Vector3f(1, 0, 0);
        } else if (fabs(axis.z) > 0.99f) {
            // Z轴挤压：使用X作为right，Y作为up（与世界坐标系一致）
            right = Vector3f(1, 0, 0);
            up = Vector3f(0, 1, 0);
        } else {
            // 任意轴向：构建正交坐标系
            Vector3f ref = (fabs(axis.x) < 0.9f) ? Vector3f(1, 0, 0) : Vector3f(0, 1, 0);
            right = glm::normalize(glm::cross(ref, axis));
            up = glm::cross(axis, right);
        }
    }

    // Helper: fill indices for extruded polygon for arbitrary index type
    template<typename IndexT>
    void FillExtrudedPolygonIndices(IndexT *ip,
                                    const ExtrudedPolygonCreateInfo *epci,
                                    uint vertexCount,
                                    bool generateSides,
                                    bool generateCaps,
                                    const Vector3f &right,
                                    const Vector3f &up,
                                    const Vector3f &forward,
                                    const Vector3f &extrudeOffset,
                                    const Vector3f &polygonCenter)
    {
        uint indexOffset = 0;

        if (generateSides)
        {
            for (uint i = 0; i < vertexCount; i++)
            {
                uint next = (i + 1) % vertexCount;

                IndexT i0 = (IndexT)(indexOffset + i * 2);
                IndexT i1 = (IndexT)(indexOffset + i * 2 + 1);
                IndexT i2 = (IndexT)(indexOffset + next * 2);
                IndexT i3 = (IndexT)(indexOffset + next * 2 + 1);

                Vector3f p0 = right * epci->vertices[i].x + up * epci->vertices[i].y;
                Vector3f p1 = p0 + extrudeOffset;
                Vector3f p2 = right * epci->vertices[next].x + up * epci->vertices[next].y;
                Vector3f p3 = p2 + extrudeOffset;

                Vector3f edge = p2 - p0;
                Vector3f sideNormal = glm::normalize(glm::cross(edge, forward));
                Vector3f midPoint = p0 + edge * 0.5f;
                Vector3f toCenter = midPoint - polygonCenter;
                if (glm::dot(sideNormal, toCenter) < 0.0f) sideNormal = sideNormal * -1.0f;

                Vector3f triA_n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                if (glm::dot(triA_n, sideNormal) >= 0.0f) {
                    *ip++ = i0; *ip++ = i1; *ip++ = i2;
                } else {
                    *ip++ = i0; *ip++ = i2; *ip++ = i1;
                }

                Vector3f triB_n = glm::normalize(glm::cross(p1 - p2, p3 - p2));
                if (glm::dot(triB_n, sideNormal) >= 0.0f) {
                    *ip++ = i2; *ip++ = i1; *ip++ = i3;
                } else {
                    *ip++ = i2; *ip++ = i3; *ip++ = i1;
                }
            }

            indexOffset += vertexCount * 2;
        }

        if (generateCaps)
        {
            IndexT bottomStart = (IndexT)indexOffset;

            Vector3f b0 = right * epci->vertices[0].x + up * epci->vertices[0].y;
            Vector3f b1 = right * epci->vertices[1].x + up * epci->vertices[1].y;
            Vector3f b2 = right * epci->vertices[2].x + up * epci->vertices[2].y;
            Vector3f triNormal = glm::normalize(glm::cross(b1 - b0, b2 - b0));
            Vector3f expectedBottomNormal = forward * -1.0f;
            bool flipBottom = (glm::dot(triNormal, expectedBottomNormal) < 0.0f);

            for (uint i = 1; i < vertexCount - 1; i++) {
                if (!flipBottom) {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + (IndexT)(i + 1);
                    *ip++ = bottomStart + (IndexT)i;
                } else {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + (IndexT)i;
                    *ip++ = bottomStart + (IndexT)(i + 1);
                }
            }

            IndexT topStart = (IndexT)(indexOffset + vertexCount);

            Vector3f t0 = (right * epci->vertices[0].x + up * epci->vertices[0].y) + extrudeOffset;
            Vector3f t1 = (right * epci->vertices[1].x + up * epci->vertices[1].y) + extrudeOffset;
            Vector3f t2 = (right * epci->vertices[2].x + up * epci->vertices[2].y) + extrudeOffset;
            Vector3f triNormalTop = glm::normalize(glm::cross(t1 - t0, t2 - t0));
            Vector3f expectedTopNormal = forward;
            bool flipTop = (glm::dot(triNormalTop, expectedTopNormal) < 0.0f);

            for (uint i = 1; i < vertexCount - 1; i++) {
                if (!flipTop) {
                    *ip++ = topStart;
                    *ip++ = topStart + (IndexT)i;
                    *ip++ = topStart + (IndexT)(i + 1);
                } else {
                    *ip++ = topStart;
                    *ip++ = topStart + (IndexT)(i + 1);
                    *ip++ = topStart + (IndexT)i;
                }
            }
        }
    }

    Geometry *CreateExtrudedPolygon(GeometryCreater *pc, const ExtrudedPolygonCreateInfo *epci)
    {
        if (!pc || !epci || !epci->vertices || epci->vertexCount < 3)
            return nullptr;

        const uint vertexCount = epci->vertexCount;
        const bool generateCaps = epci->generateCaps;
        const bool generateSides = epci->generateSides;

        // 计算顶点和索引数量
        uint totalVertices = 0;
        uint totalIndices = 0;

        if (generateSides) {
            totalVertices += vertexCount * 2;  // 侧面顶点（每个原始顶点对应上下两个3D顶点）
            totalIndices += vertexCount * 6;   // 每个侧面四边形用2个三角形，6个索引
        }

        if (generateCaps) {
            totalVertices += vertexCount * 2;  // 顶底面顶点
            totalIndices += (vertexCount - 2) * 6;  // 每个面用三角扇形分解：(n-2)个三角形 * 2个面 * 3个索引
        }

        if (!pc->Init("ExtrudedPolygon", totalVertices, totalIndices, IndexType::U32))
            return nullptr;

        VABMapFloat vertex_map(pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat normal_map(pc->GetVABMap(VAN::Normal), VF_V3F);

        if (!vertex_map) return nullptr;

        float *vp = vertex_map;
        float *np = normal_map;

        // 构建局部坐标系
        Vector3f right, up;
        BuildLocalCoordinateSystem(epci->extrudeAxis, right, up);
        Vector3f forward = glm::normalize(epci->extrudeAxis);

        // 计算挤压的起始和结束位置
        Vector3f extrudeOffset = forward * epci->extrudeDistance;

        // 计算多边形中心（用于判断侧面法线朝外）
        Vector3f polygonCenter(0.0f, 0.0f, 0.0f);
        for (uint i = 0; i < vertexCount; ++i) {
            const Vector2f &v2d = epci->vertices[i];
            polygonCenter += right * v2d.x + up * v2d.y;
        }
        polygonCenter = polygonCenter * (1.0f / static_cast<float>(vertexCount));

        // 计算输入多边形的2D有向面积以判断顶点顺序（顺时针或逆时针）
        double area2 = 0.0;
        for (uint i = 0; i < vertexCount; ++i) {
            uint j = (i + 1) % vertexCount;
            const Vector2f &a = epci->vertices[i];
            const Vector2f &b = epci->vertices[j];
            area2 += static_cast<double>(a.x) * static_cast<double>(b.y) - static_cast<double>(b.x) * static_cast<double>(a.y);
        }
        // area2 > 0 => CCW, area2 < 0 => CW
        const bool polygonIsClockwise = (area2 < 0.0);

        uint vertexIndex = 0;

        // 生成侧面顶点
        if (generateSides){
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];

                // 将2D顶点转换为3D世界坐标
                Vector3f v3d = right * v2d.x + up * v2d.y;

                // 底面顶点
                if (vp) {
                    *vp++ = v3d.x;
                    *vp++ = v3d.y;
                    *vp++ = v3d.z;
                }

                // 顶面顶点
                Vector3f topVertex = v3d + extrudeOffset;
                if (vp) {
                    *vp++ = topVertex.x;
                    *vp++ = topVertex.y;
                    *vp++ = topVertex.z;
                }

                // 计算侧面法线
                if (np) {
                    // 计算边的方向与下一个顶点
                    uint nextIndex = (i + 1) % vertexCount;
                    const Vector2f &next2d = epci->vertices[nextIndex];
                    Vector3f nextV3d = right * next2d.x + up * next2d.y;

                    Vector3f edge3d = nextV3d - v3d;

                    // 侧面法线 = edge × extrudeAxis
                    Vector3f sideNormal = glm::normalize(glm::cross(edge3d, forward));

                    // 确保法线指向外侧：通过与边中点到多边形中心的向量点乘判断
                    Vector3f midPoint = v3d + edge3d * 0.5f;
                    Vector3f toCenter = midPoint - polygonCenter;
                    if (glm::dot(sideNormal, toCenter) < 0.0f) {
                        sideNormal = sideNormal * -1.0f;
                    }

                    // 底面顶点法线
                    *np++ = sideNormal.x;
                    *np++ = sideNormal.y;
                    *np++ = sideNormal.z;

                    // 顶面顶点法线
                    *np++ = sideNormal.x;
                    *np++ = sideNormal.y;
                    *np++ = sideNormal.z;
                }

                vertexIndex += 2;
            }
        }

        // 生成顶底面顶点
        if (generateCaps) {
            // 底面顶点
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;

                if (vp) {
                    *vp++ = v3d.x;
                    *vp++ = v3d.y;
                    *vp++ = v3d.z;
                }

                if (np) {
                    // 底面法线固定为 -forward（朝下）
                    Vector3f bottomNormal = forward * -1.0f;
                    *np++ = bottomNormal.x;
                    *np++ = bottomNormal.y;
                    *np++ = bottomNormal.z;
                }
            }

            // 顶面顶点
            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;
                Vector3f topVertex = v3d + extrudeOffset;

                if (vp) {
                    *vp++ = topVertex.x;
                    *vp++ = topVertex.y;
                    *vp++ = topVertex.z;
                }

                if (np) {
                    // 顶面法线固定为 forward（朝上）
                    Vector3f topNormal = forward;
                    *np++ = topNormal.x;
                    *np++ = topNormal.y;
                    *np++ = topNormal.z;
                }
            }
        }

        // 生成索引：根据 IndexType 选择对应大小的索引缓冲写入
        const IndexType index_type = pc->GetIndexType();

        // Use the helper template to fill indices for the correct type
        switch (index_type)
        {
            case IndexType::U16:
            {
                IBTypeMap<uint16> ib_map(pc->GetIBMap());
                uint16 *ip = ib_map;
                if (totalIndices > 0 && ip == nullptr) return nullptr;

                FillExtrudedPolygonIndices(ip, epci, vertexCount, generateSides, generateCaps, right, up, forward, extrudeOffset, polygonCenter);
                break;
            }
            case IndexType::U32:
            {
                IBTypeMap<uint32> ib_map(pc->GetIBMap());
                uint32 *ip = ib_map;
                if (totalIndices > 0 && ip == nullptr) return nullptr;

                FillExtrudedPolygonIndices(ip, epci, vertexCount, generateSides, generateCaps, right, up, forward, extrudeOffset, polygonCenter);
                break;
            }
            case IndexType::U8:
            {
                IBTypeMap<uint8> ib_map(pc->GetIBMap());
                uint8 *ip = ib_map;
                if (totalIndices > 0 && ip == nullptr) return nullptr;

                FillExtrudedPolygonIndices(ip, epci, vertexCount, generateSides, generateCaps, right, up, forward, extrudeOffset, polygonCenter);
                break;
            }
            default:
                // Unsupported index type
                return nullptr;
        }

        Geometry *p = pc->Create();

        if (p) {
            // 计算包围盒
            Vector3f minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
            Vector3f maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

            for (uint i = 0; i < vertexCount; i++) {
                const Vector2f& v2d = epci->vertices[i];
                Vector3f v3d = right * v2d.x + up * v2d.y;

                // 底面顶点
                minBounds.x = std::min(minBounds.x, v3d.x);
                minBounds.y = std::min(minBounds.y, v3d.y);
                minBounds.z = std::min(minBounds.z, v3d.z);
                maxBounds.x = std::max(maxBounds.x, v3d.x);
                maxBounds.y = std::max(maxBounds.y, v3d.y);
                maxBounds.z = std::max(maxBounds.z, v3d.z);

                // 顶面顶点
                Vector3f topVertex = v3d + extrudeOffset;
                minBounds.x = std::min(minBounds.x, topVertex.x);
                minBounds.y = std::min(minBounds.y, topVertex.y);
                minBounds.z = std::min(minBounds.z, topVertex.z);
                maxBounds.x = std::max(maxBounds.x, topVertex.x);
                maxBounds.y = std::max(maxBounds.y, topVertex.y);
                maxBounds.z = std::max(maxBounds.z, topVertex.z);
            }

            BoundingVolumes bv;

            bv.SetFromAABB(minBounds,maxBounds);

            p->SetBoundingVolumes(bv);
        }

        return p;
    }

    Geometry *CreateExtrudedRectangle(GeometryCreater *pc, float width, float height, float depth, const Vector3f &extrudeAxis)
    {
        // 创建矩形顶点（中心在原点）
        Vector2f rectVertices[4] = {
            {-width * 0.5f, -height * 0.5f},  // 左下
            { width * 0.5f, -height * 0.5f},  // 右下
            { width * 0.5f,  height * 0.5f},  // 右上
            {-width * 0.5f,  height * 0.5f}   // 左上
        };

        ExtrudedPolygonCreateInfo epci;
        epci.vertices = rectVertices;
        epci.vertexCount = 4;
        epci.extrudeDistance = depth;
        epci.extrudeAxis = extrudeAxis;
        epci.generateCaps = true;
        epci.generateSides = true;
        epci.clockwiseFront = true;

        return CreateExtrudedPolygon(pc, &epci);
    }

    Geometry *CreateExtrudedCircle(GeometryCreater *pc, float radius, float height, uint segments, const Vector3f &extrudeAxis)
    {
        if (segments < 3) segments = 3;

        // 创建圆形顶点（顺时针顺序，从+X轴开始）
        std::vector<Vector2f> circleVertices(segments);
        float angleStep = 2.0f * std::numbers::pi_v<float> / segments;

        for (uint i = 0; i < segments; i++) {
            // 注意：使用负角度来确保顺时针顺序
            float angle = -(float)i * angleStep;  // 显式转换为float避免精度问题
            circleVertices[i].x = cos(angle) * radius;
            circleVertices[i].y = sin(angle) * radius;
        }

        ExtrudedPolygonCreateInfo epci;
        epci.vertices = circleVertices.data();
        epci.vertexCount = segments;
        epci.extrudeDistance = height;
        epci.extrudeAxis = extrudeAxis;
        epci.generateCaps = true;
        epci.generateSides = true;
        epci.clockwiseFront = true;

        return CreateExtrudedPolygon(pc, &epci);
    }
}//namespace hgl::graph::inline_geometry
