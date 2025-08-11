#pragma once

#include <hgl/io/event/WindowEvent.h>

namespace hgl::io
{
    /**
     * 简单的2D坐标结构
     */
    struct Vector2i
    {
        int x, y;
        Vector2i() : x(0), y(0) {}
        Vector2i(int x_, int y_) : x(x_), y(y_) {}
        void Set(int x_, int y_) { x = x_; y = y_; }
        Vector2i operator-(const Vector2i& other) const { return Vector2i(x - other.x, y - other.y); }
    };

    /**
     * 鼠标事件处理类
     */
    class MouseEvent : public WindowEvent
    {
        Vector2i mouse_coord;       ///< 鼠标坐标
        Vector2i last_mouse_coord;  ///< 上次鼠标坐标
        bool button_states[3] = {false, false, false}; ///< 鼠标按键状态 [左, 右, 中]

    public:
        MouseEvent() = default;
        virtual ~MouseEvent() = default;

        /**
         * 获取鼠标坐标
         */
        const Vector2i& GetMouseCoord() const { return mouse_coord; }

        /**
         * 获取上次鼠标坐标
         */
        const Vector2i& GetLastMouseCoord() const { return last_mouse_coord; }

        /**
         * 获取鼠标移动距离
         */
        Vector2i GetMouseDelta() const { return mouse_coord - last_mouse_coord; }

        /**
         * 获取鼠标按键状态
         */
        bool GetButtonState(uint button) const
        {
            return button < 3 ? button_states[button] : false;
        }

        /**
         * 设置鼠标坐标
         */
        void SetMouseCoord(int x, int y)
        {
            last_mouse_coord = mouse_coord;
            mouse_coord.Set(x, y);
        }

        /**
         * 处理鼠标移动事件
         */
        void OnMouseMove(int x, int y) override
        {
            SetMouseCoord(x, y);
        }

        /**
         * 处理鼠标按下事件
         */
        void OnMouseDown(uint button) override
        {
            if (button < 3)
            {
                button_states[button] = true;
            }
        }

        /**
         * 处理鼠标释放事件
         */
        void OnMouseUp(uint button) override
        {
            if (button < 3)
            {
                button_states[button] = false;
            }
        }

        /**
         * 更新鼠标状态（在帧循环中调用）
         */
        virtual void Update()
        {
            // 在这里可以处理鼠标状态的持续更新
        }

    protected:
        /**
         * 进入独占模式时的处理
         */
        void OnEnterExclusiveMode(EventDispatcher* exclusive_dispatcher) override
        {
            // 可以在这里添加鼠标独占模式的特殊处理
        }

        /**
         * 退出独占模式时的处理
         */
        void OnExitExclusiveMode(EventDispatcher* old_exclusive_dispatcher) override
        {
            // 可以在这里添加退出鼠标独占模式的处理
        }
    };

} // namespace hgl::io