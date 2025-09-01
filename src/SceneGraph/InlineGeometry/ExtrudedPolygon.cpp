// GL to VK: swap Y/Z of position/normal/tangent/index

#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/PrimitiveCreater.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace hgl::graph::inline_geometry
{
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

    Primitive *CreateExtrudedPolygon(PrimitiveCreater *pc, const ExtrudedPolygonCreateInfo *epci)
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
                    // 计算边的方向
                    uint nextIndex = (i + 1) % vertexCount;
                    Vector2f edge2d = epci->vertices[nextIndex] - v2d;
                    Vector3f edge3d = right * edge2d.x + up * edge2d.y;

                    // 侧面法线 = edge × extrudeAxis
                    Vector3f sideNormal = glm::normalize(glm::cross(edge3d, forward));

                    // 确保法线指向外侧
                    if (!epci->clockwiseFront) {
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
                    Vector3f bottomNormal = forward * -1.0f;
                    if (!epci->clockwiseFront) {
                        bottomNormal = bottomNormal * -1.0f;
                    }
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
                    Vector3f topNormal = forward;
                    if (!epci->clockwiseFront) {
                        topNormal = topNormal * -1.0f;
                    }
                    *np++ = topNormal.x;
                    *np++ = topNormal.y;
                    *np++ = topNormal.z;
                }
            }
        }

        // 生成索引
        IBTypeMap<uint32> ib_map(pc->GetIBMap());
        uint32 *ip = ib_map;

        uint indexOffset = 0;

        // 生成侧面索引
        if (generateSides) {
            for (uint i = 0; i < vertexCount; i++) {
                uint next = (i + 1) % vertexCount;

                uint i0 = indexOffset + i * 2;      // 当前底面顶点
                uint i1 = indexOffset + i * 2 + 1;  // 当前顶面顶点
                uint i2 = indexOffset + next * 2;   // 下一个底面顶点
                uint i3 = indexOffset + next * 2 + 1; // 下一个顶面顶点

                if (epci->clockwiseFront) {
                    // 第一个三角形 (底面 -> 顶面 -> 下一个底面)
                    *ip++ = i0; *ip++ = i1; *ip++ = i2;
                    // 第二个三角形 (下一个底面 -> 顶面 -> 下一个顶面)
                    *ip++ = i2; *ip++ = i1; *ip++ = i3;
                } else {
                    // 逆时针顶点顺序
                    *ip++ = i0; *ip++ = i2; *ip++ = i1;
                    *ip++ = i2; *ip++ = i3; *ip++ = i1;
                }
            }
            indexOffset += vertexCount * 2;
        }

        // 生成顶底面索引
        if (generateCaps) {
            // 底面索引（三角扇形）
            uint bottomStart = indexOffset;

            // 采样前三个底面顶点，计算当前三角形法线，与期望底面法线比较决定是否反转索引顺序
            Vector3f b0 = right * epci->vertices[0].x + up * epci->vertices[0].y;
            Vector3f b1 = right * epci->vertices[1].x + up * epci->vertices[1].y;
            Vector3f b2 = right * epci->vertices[2].x + up * epci->vertices[2].y;
            Vector3f triNormal = glm::normalize(glm::cross(b1 - b0, b2 - b0));

            Vector3f expectedBottomNormal = forward * -1.0f;
            if (!epci->clockwiseFront) expectedBottomNormal = expectedBottomNormal * -1.0f;

            bool flipBottom = (glm::dot(triNormal, expectedBottomNormal) < 0.0f);

            for (uint i = 1; i < vertexCount - 1; i++) {
                if (!flipBottom) {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + i + 1;
                    *ip++ = bottomStart + i;
                } else {
                    *ip++ = bottomStart;
                    *ip++ = bottomStart + i;
                    *ip++ = bottomStart + i + 1;
                }
            }

            // 顶面索引（三角扇形）
            uint topStart = indexOffset + vertexCount;

            // 采样前三个顶面顶点
            Vector3f t0 = (right * epci->vertices[0].x + up * epci->vertices[0].y) + extrudeOffset;
            Vector3f t1 = (right * epci->vertices[1].x + up * epci->vertices[1].y) + extrudeOffset;
            Vector3f t2 = (right * epci->vertices[2].x + up * epci->vertices[2].y) + extrudeOffset;
            Vector3f triNormalTop = glm::normalize(glm::cross(t1 - t0, t2 - t0));

            Vector3f expectedTopNormal = forward;
            if (!epci->clockwiseFront) expectedTopNormal = expectedTopNormal * -1.0f;

            bool flipTop = (glm::dot(triNormalTop, expectedTopNormal) < 0.0f);

            for (uint i = 1; i < vertexCount - 1; i++) {
                if (!flipTop) {
                    *ip++ = topStart;
                    *ip++ = topStart + i;
                    *ip++ = topStart + i + 1;
                } else {
                    *ip++ = topStart;
                    *ip++ = topStart + i + 1;
                    *ip++ = topStart + i;
                }
            }
        }

        Primitive *p = pc->Create();

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

            p->SetBoundingBox(minBounds, maxBounds);
        }

        return p;
    }

    Primitive *CreateExtrudedRectangle(PrimitiveCreater *pc, float width, float height, float depth, const Vector3f &extrudeAxis)
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

    Primitive *CreateExtrudedCircle(PrimitiveCreater *pc, float radius, float height, uint segments, const Vector3f &extrudeAxis)
    {
        if (segments < 3) segments = 3;

        // 创建圆形顶点（顺时针顺序，从+X轴开始）
        std::vector<Vector2f> circleVertices(segments);
        float angleStep = 2.0f * HGL_PI / segments;

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
