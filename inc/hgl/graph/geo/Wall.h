#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        // 线段现在使用顶点索引表示，不直接保存坐标
        struct Line2D
        {
            uint a;    // 起点索引
            uint b;    // 终点索引

            Line2D() = default;
            Line2D(uint a_, uint b_) : a(a_), b(b_) {}
        };

        struct WallCreateInfo
        {
            // 顶点数组（2D坐标）
            const Vector2f* vertices = nullptr;  // 顶点数据
            uint vertexCount = 0;                // 顶点数量

            // 索引数组：按两两成对表示一条线段（推荐使用）
            const uint* indices = nullptr;       // 索引数组（每2个索引表示一条线段）
            uint indexCount = 0;                 // 索引数量，应为线段数 * 2

            // 备选（兼容）接口：直接传入 Line2D 数组，但 Line2D 中存储的是索引而非坐标
            const Line2D* lines = nullptr;      // 每项为一条线段的索引对
            uint lineCount = 0;                 // 线段数量

            float thickness = 0.1f;            // 墙壁厚度
            float height = 1.0f;               // 墙壁高度

            // 角落连接方式
            enum class CornerJoin
            {
                Miter = 0,
                Bevel = 1,
                Round = 2
            };

            CornerJoin cornerJoin = CornerJoin::Miter; // 默认使用斜接
            float miterLimit = 4.0f; // 当 miter 长度超过 miterLimit * halfThickness 时退化为 bevel
            uint roundSegments = 6; // 圆角分段数

            // UV 平铺参数
            float uv_tile_v = 1.0f;                // 纵向（高度）平铺次数
            float uv_u_repeat_per_unit = 1.0f;     // 横向沿长度方向的重复次数每单位长度

            // 可选参数
            bool generateUV = true;     // 是否生成UV坐标
            bool generateNormals = true; // 是否生成法线
            bool generateTangents = true; // 是否生成切线

            WallCreateInfo() = default;
        };

        /**
         * 创建2D线条转换的3D墙壁(三角形)
         * 输入方式（优先级）：
         * 1. vertices + indices（indices 按两两成对表示线段）
         * 2. vertices + lines（lines 中为索引对）
         *
         * 注意：Line2D 不再保存坐标，使用索引引用 vertices 中的坐标。
         */
        Geometry *CreateWallsFromLines2D(GeometryCreater *pc, const WallCreateInfo *wci);

        /**
         * 便利函数：创建单条水平墙体
         */
        inline Geometry *CreateHorizontalWall(GeometryCreater *pc, float startX, float startY,
                                             float length, float thickness = 0.2f, float height = 3.0f)
        {
            // 方便性函数仍使用旧逻辑，构建顶点和索引并调用主接口
            static Vector2f verts[2];
            static uint idx[2];

            verts[0] = Vector2f(startX, startY);
            verts[1] = Vector2f(startX + length, startY);
            idx[0] = 0; idx[1] = 1;

            WallCreateInfo wci;
            wci.vertices = verts; wci.vertexCount = 2;
            wci.indices = idx; wci.indexCount = 2;
            wci.thickness = thickness; wci.height = height;

            return CreateWallsFromLines2D(pc, &wci);
        }

        /**
         * 便利函数：创建单条垂直墙体
         */
        inline Geometry *CreateVerticalWall(GeometryCreater *pc, float startX, float startY,
                                           float length, float thickness = 0.2f, float height = 3.0f)
        {
            static Vector2f verts[2];
            static uint idx[2];

            verts[0] = Vector2f(startX, startY);
            verts[1] = Vector2f(startX, startY + length);
            idx[0] = 0; idx[1] = 1;

            WallCreateInfo wci;
            wci.vertices = verts; wci.vertexCount = 2;
            wci.indices = idx; wci.indexCount = 2;
            wci.thickness = thickness; wci.height = height;

            return CreateWallsFromLines2D(pc, &wci);
        }
    }//namespace inline_geometry
}//namespace hgl::graph
