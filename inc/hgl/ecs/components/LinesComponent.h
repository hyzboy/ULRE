#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>
#include<vector>

namespace hgl::ecs
{
    /**
     * CN: 线条组件 - ECS方式管理线段数据
     * EN: Lines Component - Manage line segments in ECS manner
     *
     * CN: 每个LinesComponent代表一个物体的所有线条，这些线条共享同一个宽度值。
     * EN: Each LinesComponent represents all lines of an entity, sharing a single width value.
     */
    struct LinesComponent : public Component
    {
    public:
        struct LineSegment
        {
            hgl::math::Vector3f from;       ///< CN: 起始点 EN: Start point
            hgl::math::Vector3f to;         ///< CN: 结束点 EN: End point
            uint8_t color_index;            ///< CN: 颜色索引(0-255) EN: Color index (0-255)
        };

        std::vector<LineSegment> lines;      ///< CN: 线段列表 EN: Line segments
        uint8_t width = 1;                   ///< CN: 线宽(1-16) EN: Line width (1-16)
        bool visible = true;                 ///< CN: 是否可见 EN: Visibility
        bool dirty = true;                   ///< CN: 是否需要同步 EN: Need sync to renderer

    public:

        LinesComponent() = default;
        virtual ~LinesComponent() = default;

        const char* GetRenderSystemGroupName() const override { return "Line"; }

        /**
         * CN: 添加一条线段
         * EN: Add a line segment
         */
        void AddLine(const hgl::math::Vector3f &from, const hgl::math::Vector3f &to, uint8_t color_index = 0)
        {
            lines.push_back({from, to, color_index});
            dirty = true;
        }

        /**
         * CN: 清空所有线段
         * EN: Clear all line segments
         */
        void ClearLines()
        {
            lines.clear();
            dirty = true;
        }

        /**
         * CN: 获取线段数量
         * EN: Get line count
         */
        size_t GetLineCount() const { return lines.size(); }

        /**
         * CN: 是否有线段
         * EN: Has any lines
         */
        bool HasLines() const { return !lines.empty(); }

        /**
         * CN: 设置线宽
         * EN: Set line width
         */
        void SetWidth(uint8_t w)
        {
            if (width != w)
            {
                width = w;
                dirty = true;
            }
        }

        /**
         * CN: 标记为已同步
         * EN: Mark as synced
         */
        void MarkSynced() { dirty = false; }
    };
}
