#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/math/Vector.h>
#include<hgl/math/geometry/AABB.h>
#include<hgl/type/ValueArray.h>

namespace hgl::ecs
{
    /**
     * CN: 原生线段组件 - ECS方式管理 1px 标准 Vulkan Line 的线段数据
     * EN: Simple lines component - manage line segments rendered as native 1px Vulkan lines
     *
     * CN: 与 LinesComponent 的区别：走 LineList mesh 发射（layout(lines) 原生线图元），
     *     光栅化与经典 LINE_LIST 逐像素一致；**没有宽度概念**（恒 1px），
     *     因此不携带 width/style/min_width，顶点流也没有 Size。
     * EN: Unlike LinesComponent (LineQuad screen-space quads), segments are emitted as
     *     native line primitives; width is always 1px, so no width/style/min_width data.
     */
    struct SimpleLinesComponent : public ComponentRow
    {
    public:
        struct SimpleLineSegment
        {
            hgl::math::Vector3f from;       ///< CN: 起始点 EN: Start point
            hgl::math::Vector3f to;         ///< CN: 结束点 EN: End point
            uint8_t color_index;            ///< CN: 颜色索引(0-255) EN: Color index (0-255)
        };

        ValueArray<SimpleLineSegment> lines;   ///< CN: 线段列表 EN: Line segments

    private:

        hgl::math::AABB local_bounds;
        bool local_bounds_valid = false;

    public:

        SimpleLinesComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~SimpleLinesComponent() = default;

        // 系统组（`Line`）由**类型表**声明（`kComponentTypeTable[ComponentType::SimpleLines].group`）。

        /**
         * CN: 添加一条线段
         * EN: Add a line segment
         */
        void AddLine(const hgl::math::Vector3f &from, const hgl::math::Vector3f &to, uint8_t color_index = 0)
        {
            lines.Add(SimpleLineSegment{from, to, color_index});
            local_bounds_valid = false;
        }

        /**
         * CN: 清空所有线段
         * EN: Clear all line segments
         */
        void ClearLines()
        {
            lines.Clear();
            local_bounds_valid = false;
        }

        /// CN: 获取线段数量
        /// EN: Get line count
        size_t GetLineCount() const { return static_cast<size_t>(lines.GetCount()); }

        /// CN: 是否有线段
        /// EN: Has any lines
        bool HasLines() const { return !lines.IsEmpty(); }

        bool HasValidLocalBounds() const { return local_bounds_valid; }

        const hgl::math::AABB& GetLocalBounds() const { return local_bounds; }

        bool RecalculateLocalBounds()
        {
            if (lines.IsEmpty())
            {
                local_bounds_valid = false;
                return false;
            }

            // 以首段两端点初始化 min/max，逐项手工比较（不依赖 std::min/max/numeric_limits）
            const SimpleLineSegment &first = lines[0];
            float min_x = first.from.x < first.to.x ? first.from.x : first.to.x;
            float min_y = first.from.y < first.to.y ? first.from.y : first.to.y;
            float min_z = first.from.z < first.to.z ? first.from.z : first.to.z;
            float max_x = first.from.x < first.to.x ? first.to.x : first.from.x;
            float max_y = first.from.y < first.to.y ? first.to.y : first.from.y;
            float max_z = first.from.z < first.to.z ? first.to.z : first.from.z;

            for (int i = 1; i < lines.GetCount(); ++i)
            {
                const SimpleLineSegment &segment = lines[i];

                if (segment.from.x < min_x) min_x = segment.from.x;
                if (segment.from.y < min_y) min_y = segment.from.y;
                if (segment.from.z < min_z) min_z = segment.from.z;
                if (segment.to.x   < min_x) min_x = segment.to.x;
                if (segment.to.y   < min_y) min_y = segment.to.y;
                if (segment.to.z   < min_z) min_z = segment.to.z;

                if (segment.from.x > max_x) max_x = segment.from.x;
                if (segment.from.y > max_y) max_y = segment.from.y;
                if (segment.from.z > max_z) max_z = segment.from.z;
                if (segment.to.x   > max_x) max_x = segment.to.x;
                if (segment.to.y   > max_y) max_y = segment.to.y;
                if (segment.to.z   > max_z) max_z = segment.to.z;
            }

            local_bounds.SetMinMax(hgl::math::Vector3f(min_x, min_y, min_z),
                                   hgl::math::Vector3f(max_x, max_y, max_z));
            local_bounds_valid = true;
            return true;
        }

    };

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<SimpleLinesComponent> { static constexpr ComponentType value = ComponentType::SimpleLines; };
}
