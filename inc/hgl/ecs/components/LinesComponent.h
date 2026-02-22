#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/math/Vector.h>
#include<hgl/math/geometry/AABB.h>
#include<hgl/color/Color4f.h>
#include<vector>
#include<limits>
#include<algorithm>

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
        enum class LineChange : uint32_t
        {
            Geometry = 1u << 0,
            Style = 1u << 1,
            Visibility = 1u << 2,
        };

        enum class LineStyle : uint8_t
        {
            Solid = 0,
            Dashed,
            Dotted,
        };

        struct LineSegment
        {
            hgl::math::Vector3f from;       ///< CN: 起始点 EN: Start point
            hgl::math::Vector3f to;         ///< CN: 结束点 EN: End point
            uint8_t color_index;            ///< CN: 颜色索引(0-255) EN: Color index (0-255)
        };

        std::vector<LineSegment> lines;      ///< CN: 线段列表 EN: Line segments
        uint8_t width = 1;                   ///< CN: 线宽(1-16) EN: Line width (1-16)
        LineStyle style = LineStyle::Solid;  ///< CN: 线条风格 EN: Line style
        bool visible = true;                 ///< CN: 是否可见 EN: Visibility
        bool dirty = true;                   ///< CN: 是否需要同步 EN: Need sync to renderer

    private:

        hgl::math::AABB local_bounds;
        bool local_bounds_valid = false;

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
            local_bounds_valid = false;
            TouchChange(static_cast<uint32_t>(LineChange::Geometry));
        }

        /**
         * CN: 清空所有线段
         * EN: Clear all line segments
         */
        void ClearLines()
        {
            lines.clear();
            dirty = true;
            local_bounds_valid = false;
            TouchChange(static_cast<uint32_t>(LineChange::Geometry));
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
                TouchChange(static_cast<uint32_t>(LineChange::Style));
            }
        }

        void SetStyle(LineStyle s)
        {
            if (style != s)
            {
                style = s;
                dirty = true;
                TouchChange(static_cast<uint32_t>(LineChange::Style));
            }
        }

        LineStyle GetStyle() const { return style; }

        void SetVisible(bool value)
        {
            if (visible != value)
            {
                visible = value;
                dirty = true;
                TouchChange(static_cast<uint32_t>(LineChange::Visibility));
            }
        }

        bool HasValidLocalBounds() const { return local_bounds_valid; }

        const hgl::math::AABB& GetLocalBounds() const { return local_bounds; }

        bool RecalculateLocalBounds()
        {
            if (lines.empty())
            {
                local_bounds_valid = false;
                return false;
            }

            float min_x = std::numeric_limits<float>::max();
            float min_y = std::numeric_limits<float>::max();
            float min_z = std::numeric_limits<float>::max();
            float max_x = -std::numeric_limits<float>::max();
            float max_y = -std::numeric_limits<float>::max();
            float max_z = -std::numeric_limits<float>::max();

            for (const auto& segment : lines)
            {
                min_x = std::min(min_x, std::min(segment.from.x, segment.to.x));
                min_y = std::min(min_y, std::min(segment.from.y, segment.to.y));
                min_z = std::min(min_z, std::min(segment.from.z, segment.to.z));
                max_x = std::max(max_x, std::max(segment.from.x, segment.to.x));
                max_y = std::max(max_y, std::max(segment.from.y, segment.to.y));
                max_z = std::max(max_z, std::max(segment.from.z, segment.to.z));
            }

            local_bounds.SetMinMax(hgl::math::Vector3f(min_x, min_y, min_z),
                                   hgl::math::Vector3f(max_x, max_y, max_z));
            local_bounds_valid = true;
            return true;
        }

        /**
         * CN: 标记为已同步
         * EN: Mark as synced
         */
        void MarkSynced() { dirty = false; }
    };
}
