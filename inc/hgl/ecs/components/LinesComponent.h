#pragma once

#include<hgl/ecs/core/ComponentRow.h>
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
    struct LinesComponent : public ComponentRow
    {
    public:
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
            float min_width = 0.0f;         ///< CN: 最细阈值(像素)——深度衰减下限；0=可细到消失 EN: Min width (px), depth-attenuation floor
        };

        std::vector<LineSegment> lines;      ///< CN: 线段列表 EN: Line segments
        uint8_t width = 1;                   ///< CN: 线宽(1-16) EN: Line width (1-16)
        LineStyle style = LineStyle::Solid;  ///< CN: 线条风格 EN: Line style

    private:

        hgl::math::AABB local_bounds;
        bool local_bounds_valid = false;

    public:

        LinesComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~LinesComponent() = default;

        // 系统组（`Line`）由**类型表**声明（`kComponentTypeTable[ComponentType::Lines].group`）：
        // T2-c 起组归属是类型属性，不再由组件对象上的虚函数声明（虚表已随基类删除）。

        /**
         * CN: 添加一条线段
         * EN: Add a line segment
         */
        void AddLine(const hgl::math::Vector3f &from, const hgl::math::Vector3f &to, uint8_t color_index = 0)
        {
            lines.push_back({from, to, color_index});
            local_bounds_valid = false;
        }

        /**
         * CN: 清空所有线段
         * EN: Clear all line segments
         */
        void ClearLines()
        {
            lines.clear();
            local_bounds_valid = false;
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
            }
        }

        void SetStyle(LineStyle s)
        {
            if (style != s)
            {
                style = s;
            }
        }

        LineStyle GetStyle() const { return style; }

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

    };

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<LinesComponent> { static constexpr ComponentType value = ComponentType::Lines; };
}
