#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>

namespace hgl::graph
{
    class PrimitiveCreater;

    namespace inline_geometry
    {
        // 新增：2D线条转3D墙壁
        struct Line2D
        {
            Vector2f start;    // 线段起点
            Vector2f end;      // 线段终点

            Line2D() = default;
            Line2D(const Vector2f& s, const Vector2f& e) : start(s), end(e) {}
            Line2D(float x1, float y1, float x2, float y2) : start(x1, y1), end(x2, y2) {}
        };

        struct WallCreateInfo
        {
            const Line2D* lines;        // 2D线段数组
            uint lineCount;             // 线段数量
            float thickness;            // 墙壁厚度
            float height;               // 墙壁高度

            // 可选参数
            bool generateUV = true;     // 是否生成UV坐标
            bool generateNormals = true; // 是否生成法线
            bool generateTangents = true; // 是否生成切线

            WallCreateInfo()
            {
                lines = nullptr;
                lineCount = 0;
                thickness = 0.1f;
                height = 1.0f;
            }
        };

        /**
         * 创建2D线条转换的3D墙壁(三角形)
         * 支持单条直线(生成cube)、多条线的连接(拐角、T型交叉口、十字交叉口等)
         * Z轴向上，ClockWise为正面
         *
         * 当前实现支持：
         * - 单条线段：创建一个矩形墙体(本质上是一个薄cube)
         * - 多条线段：为每条线段创建独立的墙体(可能在连接处有重叠几何)
         *
         * 注意：完整的拐角、T型和十字交叉处理需要复杂的几何算法，
         * 包括偏移线相交计算和连接处的几何优化。当前版本为基础实现。
         */
        Primitive *CreateWallsFromLines2D(PrimitiveCreater *pc, const WallCreateInfo *wci);

        /**
         * 便利函数：创建单条水平墙体
         */
        inline Primitive *CreateHorizontalWall(PrimitiveCreater *pc, float startX, float startY,
                                             float length, float thickness = 0.2f, float height = 3.0f)
        {
            Line2D line(startX, startY, startX + length, startY);
            WallCreateInfo wci;
            wci.lines = &line; wci.lineCount = 1;
            wci.thickness = thickness; wci.height = height;
            return CreateWallsFromLines2D(pc, &wci);
        }

        /**
         * 便利函数：创建单条垂直墙体
         */
        inline Primitive *CreateVerticalWall(PrimitiveCreater *pc, float startX, float startY,
                                           float length, float thickness = 0.2f, float height = 3.0f)
        {
            Line2D line(startX, startY, startX, startY + length);
            WallCreateInfo wci;
            wci.lines = &line; wci.lineCount = 1;
            wci.thickness = thickness; wci.height = height;
            return CreateWallsFromLines2D(pc, &wci);
        }
    }//namespace inline_geometry
}//namespace hgl::graph
