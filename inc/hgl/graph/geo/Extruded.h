#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Size2.h>
#include<hgl/color/Color4f.h>
#include<hgl/math/geometry/AABB.h>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        // 2D多边形挤压为3D多边形
        struct ExtrudedPolygonCreateInfo
        {
            Vector2f *vertices;             // 2D多边形顶点数组
            uint vertexCount;               // 顶点数量
            float extrudeDistance;          // 挤压距离
            Vector3f extrudeAxis;           // 挤压轴向（单位向量）
            bool generateCaps;              // 是否生成顶底面
            bool generateSides;             // 是否生成侧面
            bool clockwiseFront;            // 顶点顺序是否为顺时针为正面

            ExtrudedPolygonCreateInfo()
            {
                vertices = nullptr;
                vertexCount = 0;
                extrudeDistance = 1.0f;
                extrudeAxis = Vector3f(0, 0, 1);  // 默认Z轴向上
                generateCaps = true;
                generateSides = true;
                clockwiseFront = true;            // Z轴向上，顺时针为正面
            }
        };

        /**
         * 创建一个由2D多边形挤压生成的3D多边形(三角形)
         */
        Geometry *CreateExtrudedPolygon(GeometryCreater *pc, const ExtrudedPolygonCreateInfo *epci);

        /**
         * 创建一个由矩形挤压生成的立方体(三角形)，等价于CreateCube但使用挤压算法
         */
        Geometry *CreateExtrudedRectangle(GeometryCreater *pc, float width, float height, float depth, const Vector3f &extrudeAxis = Vector3f(0, 0, 1));

        /**
         * 创建一个由圆形挤压生成的圆柱体(三角形)，等价于CreateCylinder但使用挤压算法
         */
        Geometry *CreateExtrudedCircle(GeometryCreater *pc, float radius, float height, uint segments = 16, const Vector3f &extrudeAxis = Vector3f(0, 0, 1));
    }//namespace inline_geometry
}//namespace hgl::graph
